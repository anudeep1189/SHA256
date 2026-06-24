/*
 * app_controller.cu - Application Controller Implementation
 * Single Responsibility: Orchestrate UI, input, GPU kernel execution, and metrics collection
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#include "app_controller.h"
#include "config.h"

extern "C" {
#include "sha256.cuh"
#include "sha256_cpu.h"
}

#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>

#define SHA256_DIGEST_SIZE 32

AppController::AppController(CmdUI& ui, InputHandler& inputHandler,
							 GPUDeviceInfo& gpuInfo, PowerMetrics& metrics)
	: ui(ui)
	, inputHandler(inputHandler)
	, gpuInfo(gpuInfo)
	, metrics(metrics)
	, running(false)
{
}

AppController::~AppController()
{
}

void AppController::run()
{
	running = true;

	// Initialize UI
	ui.initialize(170, 42);
	ui.setGPUInfo(gpuInfo);
	ui.drawFrame();

	// Enter event loop with callback
	ui.runEventLoop([this](UIEvent event) {
		this->onUIEvent(event);
	});
}

void AppController::onUIEvent(UIEvent event)
{
	switch (event) {
		case UIEvent::RUN_CLICKED:
			onRunHashGPU();
			break;

		case UIEvent::RUN_CLICKED_CPU:
			onRunHashCPU();
			break;

		case UIEvent::CLEAR_CLICKED:
			ui.clearAll();
			break;

		case UIEvent::QUIT:
			running = false;
			// Force exit the event loop
			ExitProcess(0);
			break;
	}
}

void AppController::onRunHashGPU()
{
	// 1. Show processing status
	ui.drawStatusMessage("Processing... Please wait.", true, false);

	// 2. Get text input
	std::string text = ui.getTextInput();
	if (text.empty()) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Text input is empty. Please enter text to hash.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}
	std::vector<BYTE> inputData = inputHandler.readFromText(text);

	// 3. Get batch size
	int n_batch = ui.getBatchSize();

	if (n_batch <= 0) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Batch size must be greater than 0.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}

	// 4. Validate batch count against GPU memory
	// Each thread needs 32 bytes output; input is shared (single copy)
	if (gpuInfo.valid) {
		size_t perItemMem = 32;
		size_t inputMem = inputData.size();
		size_t safeLimit = (size_t)(gpuInfo.freeMemory * 0.8);
		int maxSafeBatch = (int)((safeLimit - inputMem) / perItemMem);
		if (maxSafeBatch < 1) maxSafeBatch = 1;
		if (n_batch > maxSafeBatch) {
			n_batch = maxSafeBatch;
			ui.drawStatusMessage("Clamped batch to " + std::to_string(n_batch) + " (GPU memory limit).", true, false);
		}
	}

	// 5. Prepare single-copy input buffer
	SHA_WORD inlen = (SHA_WORD)inputData.size();
	BYTE* inBuffer = (BYTE*)malloc(inlen);
	if (!inBuffer) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Failed to allocate input buffer.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}
	memcpy(inBuffer, inputData.data(), inlen);

	// 6. Allocate output buffer
	size_t totalOutputSize = (size_t)SHA256_DIGEST_SIZE * n_batch;
	BYTE* outBuffer = (BYTE*)malloc(totalOutputSize);
	if (!outBuffer) {
		free(inBuffer);
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Failed to allocate output buffer.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}
	memset(outBuffer, 0, totalOutputSize);

	// 7. Start power sampling and timer
	metrics.startSampling();
	metrics.startTimer();

	// 8. Call SHA256 CUDA kernel (nonce-based, unique hash per thread) - timed with CUDA events
	cudaEvent_t batchStart, batchStop;
	cudaEventCreate(&batchStart);
	cudaEventCreate(&batchStop);
	cudaEventRecord(batchStart);
	cudaError_t cudaErr = mcm_cuda_sha256_hash_batch(inBuffer, inlen, outBuffer, (SHA_WORD)n_batch);
	cudaEventRecord(batchStop);
	cudaEventSynchronize(batchStop);
	float batchTimeMs = 0.0f;
	cudaEventElapsedTime(&batchTimeMs, batchStart, batchStop);
	cudaEventDestroy(batchStart);
	cudaEventDestroy(batchStop);

	// 9. Stop timer and power sampling
	metrics.stopTimer();
	metrics.stopSampling();

	// 9b. Run single-hash kernel for parallel advantage comparison
	BYTE* singleOutBuffer = (BYTE*)malloc(SHA256_DIGEST_SIZE);
	float singleTimeMs = 0.0f;
	if (singleOutBuffer) {
		memset(singleOutBuffer, 0, SHA256_DIGEST_SIZE);
		cudaEvent_t singleStart, singleStop;
		cudaEventCreate(&singleStart);
		cudaEventCreate(&singleStop);
		cudaEventRecord(singleStart);
		mcm_cuda_sha256_hash_batch(inBuffer, inlen, singleOutBuffer, 1);
		cudaEventRecord(singleStop);
		cudaEventSynchronize(singleStop);
		cudaEventElapsedTime(&singleTimeMs, singleStart, singleStop);
		cudaEventDestroy(singleStart);
		cudaEventDestroy(singleStop);
		free(singleOutBuffer);
	}

	// Check for CUDA errors
	if (cudaErr != cudaSuccess) {
		free(inBuffer);
		free(outBuffer);
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = std::string("CUDA error: ") + cudaGetErrorString(cudaErr);
		ui.drawResultsPanel(errMetrics, true);
		return;
	}

	// 10. Extract sample hashes
	// Hash #0 = original SHA256 (thread 0 has no nonce appended)
	std::vector<std::pair<int, std::string>> sampleHashes;
	sampleHashes.push_back({ -1, hashToHex(outBuffer, SHA256_DIGEST_SIZE) });

	// Nonce-based samples at evenly-spaced indices (threads 1+)
	if (n_batch > 1) {
		std::vector<int> sampleIndices;
		if (n_batch <= 5) {
			for (int i = 1; i < n_batch; i++) sampleIndices.push_back(i);
		} else {
			sampleIndices.push_back(n_batch / 4);
			sampleIndices.push_back(n_batch / 2);
			sampleIndices.push_back(n_batch * 3 / 4);
			sampleIndices.push_back(n_batch - 1);
		}
		for (int idx : sampleIndices) {
			sampleHashes.push_back({ idx, hashToHex(outBuffer + (size_t)idx * SHA256_DIGEST_SIZE, SHA256_DIGEST_SIZE) });
		}
	}

	// 11. Build metrics
	size_t totalDataProcessed = (size_t)(inlen + 4) * n_batch;
	GPUMetrics result = metrics.buildMetrics(totalDataProcessed, sampleHashes[0].second);
	result.batchSize = n_batch;
	result.sampleHashes = sampleHashes;
	result.singleHashTimeMs = (double)singleTimeMs;
	result.batchHashTimeMs = (double)batchTimeMs;

	// 12. Display results
	ui.drawResultsPanel(result, true);

	// Cleanup
	free(inBuffer);
	free(outBuffer);
}

void AppController::onRunHashCPU()
{
	// 1. Show processing status
	ui.drawStatusMessage("Processing on CPU... Please wait.", false, false);

	// 2. Get text input
	std::string text = ui.getTextInput();
	if (text.empty()) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Text input is empty. Please enter text to hash.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}
	std::vector<BYTE> inputData = inputHandler.readFromText(text);

	// 3. Get batch size
	int n_batch = ui.getBatchSize();

	if (n_batch <= 0) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Batch size must be greater than 0.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}

	// 4. Prepare input buffer
	SHA_WORD inlen = (SHA_WORD)inputData.size();
	BYTE* inBuffer = (BYTE*)malloc(inlen);
	if (!inBuffer) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Failed to allocate input buffer.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}
	memcpy(inBuffer, inputData.data(), inlen);

	// 5. Allocate output buffer
	size_t totalOutputSize = (size_t)SHA256_DIGEST_SIZE * n_batch;
	BYTE* outBuffer = (BYTE*)malloc(totalOutputSize);
	if (!outBuffer) {
		free(inBuffer);
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Failed to allocate output buffer.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}
	memset(outBuffer, 0, totalOutputSize);

	// 6. Start timer for batch processing
	auto batchStartTime = std::chrono::steady_clock::now();
	int cpuErr = mcm_cpu_sha256_hash_batch(inBuffer, inlen, outBuffer, (SHA_WORD)n_batch);
	auto batchEndTime = std::chrono::steady_clock::now();
	double batchTimeMs = std::chrono::duration<double, std::milli>(batchEndTime - batchStartTime).count();

	// Check for CPU errors
	if (cpuErr != 0) {
		free(inBuffer);
		free(outBuffer);
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "CPU SHA256 batch processing failed.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}

	// 7. Single-hash timing for comparison (n_batch=1)
	BYTE* singleOutBuffer = (BYTE*)malloc(SHA256_DIGEST_SIZE);
	double singleTimeMs = 0.0;
	if (singleOutBuffer) {
		memset(singleOutBuffer, 0, SHA256_DIGEST_SIZE);
		auto singleStartTime = std::chrono::steady_clock::now();
		mcm_cpu_sha256_hash_batch(inBuffer, inlen, singleOutBuffer, 1);
		auto singleEndTime = std::chrono::steady_clock::now();
		singleTimeMs = std::chrono::duration<double, std::milli>(singleEndTime - singleStartTime).count();
		free(singleOutBuffer);
	}

	// 8. Extract sample hashes
	std::vector<std::pair<int, std::string>> sampleHashes;
	sampleHashes.push_back({ -1, hashToHex(outBuffer, SHA256_DIGEST_SIZE) });

	// Nonce-based samples at evenly-spaced indices (threads 1+)
	if (n_batch > 1) {
		std::vector<int> sampleIndices;
		if (n_batch <= 5) {
			for (int i = 1; i < n_batch; i++) sampleIndices.push_back(i);
		} else {
			sampleIndices.push_back(n_batch / 4);
			sampleIndices.push_back(n_batch / 2);
			sampleIndices.push_back(n_batch * 3 / 4);
			sampleIndices.push_back(n_batch - 1);
		}
		for (int idx : sampleIndices) {
			sampleHashes.push_back({ idx, hashToHex(outBuffer + (size_t)idx * SHA256_DIGEST_SIZE, SHA256_DIGEST_SIZE) });
		}
	}

	// 9. Build metrics (CPU-specific: skip power metrics, use timing only)
	size_t totalDataProcessed = (size_t)(inlen + 4) * n_batch;
	GPUMetrics result = {};
	result.valid = true;
	result.batchSize = n_batch;
	result.sampleHashes = sampleHashes;
	result.singleHashTimeMs = singleTimeMs;
	result.batchHashTimeMs = batchTimeMs;
	result.executionTime = batchTimeMs / 1000.0; // Convert to seconds
	result.throughput = (totalDataProcessed > 0 && batchTimeMs > 0.0) 
		? (totalDataProcessed / (1024.0 * 1024.0)) / (batchTimeMs / 1000.0)
		: 0.0;
	result.avgPowerDraw = 0.0; // CPU power not measured
	result.totalEnergy = 0.0;  // CPU energy not measured
	result.errorMessage = "";

	// 10. Display results
	ui.drawResultsPanel(result, false);

	// Cleanup
	free(inBuffer);
	free(outBuffer);
}

std::string AppController::hashToHex(const unsigned char* hash, int len)
{
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (int i = 0; i < len; i++) {
		ss << std::setw(2) << (int)hash[i];
	}
	return ss.str();
}
