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
#include <cstdlib>

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
	ui.initialize();
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
			exit(0);
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
	if (n_batch <= 0 || n_batch > 50000000) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Batch size must be between 1 and 50,000,000 for GPU.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}

	// 4. Get runs count
	int numberOfRuns = ui.getRunsCount();
	if (numberOfRuns <= 0) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Runs count must be greater than 0.";
		ui.drawResultsPanel(errMetrics, true);
		return;
	}

	// 4b. Validate GPU device capabilities and memory constraints
	std::string gpuValidationError;
	if (!validateBatchCount(gpuInfo, n_batch, inputData.size(), gpuValidationError)) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = gpuValidationError;
		ui.drawResultsPanel(errMetrics, true);
		return;
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
 
	std::vector<RunMeasurement> runsList;
	cudaError_t cudaErr = cudaSuccess;
	double totalBatchTimeMs = 0.0;
	double totalSingleTimeMs = 0.0;
	double totalRuntimeMs = 0.0;
 
	for (int r = 1; r <= numberOfRuns; ++r) {
		// Time the full batch hash
		cudaEvent_t batchStart, batchStop;
		cudaEventCreate(&batchStart);
		cudaEventCreate(&batchStop);
		cudaEventRecord(batchStart);
		
		auto runStart = std::chrono::steady_clock::now();
		cudaErr = mcm_cuda_sha256_hash_batch(inBuffer, inlen, outBuffer, (SHA_WORD)n_batch);
		cudaEventRecord(batchStop);
		cudaEventSynchronize(batchStop);
		auto runEnd = std::chrono::steady_clock::now();
		
		float batchTimeMs = 0.0f;
		cudaEventElapsedTime(&batchTimeMs, batchStart, batchStop);
		cudaEventDestroy(batchStart);
		cudaEventDestroy(batchStop);
 
		double runtimeMs = std::chrono::duration<double, std::milli>(runEnd - runStart).count();
 
		if (cudaErr != cudaSuccess) {
			break;
		}
 
		// Time a single-hash run for comparison
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
 
		double rate = (batchTimeMs > 0.0) ? (n_batch / (batchTimeMs / 1000.0)) : 0.0;
		RunMeasurement measurement = { r, (double)singleTimeMs, (double)batchTimeMs, runtimeMs, rate };
		runsList.push_back(measurement);
 
		totalBatchTimeMs += batchTimeMs;
		totalSingleTimeMs += singleTimeMs;
		totalRuntimeMs += runtimeMs;
	}
 
	// 9. Stop timer and power sampling
	metrics.stopTimer();
	metrics.stopSampling();

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
	size_t totalDataProcessed = (size_t)(inlen + 4) * n_batch * numberOfRuns;
	GPUMetrics result = metrics.buildMetrics(totalDataProcessed, sampleHashes[0].second);
	result.batchSize = n_batch;
	result.sampleHashes = sampleHashes;
	result.singleHashTimeMs = totalSingleTimeMs / numberOfRuns;
	result.batchHashTimeMs = totalBatchTimeMs / numberOfRuns;
	result.executionTime = totalRuntimeMs / 1000.0;
	result.throughput = (totalDataProcessed > 0 && totalRuntimeMs > 0.0)
		? (totalDataProcessed / (1024.0 * 1024.0)) / (totalRuntimeMs / 1000.0)
		: 0.0;
	result.runs = runsList;

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
	if (n_batch <= 0 || n_batch > 10000000) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Batch size must be between 1 and 10,000,000 for CPU.";
		ui.drawResultsPanel(errMetrics, false);
		return;
	}

	// 4. Get runs count
	int numberOfRuns = ui.getRunsCount();
	if (numberOfRuns <= 0) {
		GPUMetrics errMetrics = {};
		errMetrics.valid = false;
		errMetrics.errorMessage = "Runs count must be greater than 0.";
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
 
	int cpuErr = 0;
	std::vector<RunMeasurement> runsList;
	double totalBatchTimeMs = 0.0;
	double totalSingleTimeMs = 0.0;
 
	for (int r = 1; r <= numberOfRuns; ++r) {
		// Time the batch hash on CPU
		auto batchStartTime = std::chrono::steady_clock::now();
		cpuErr = mcm_cpu_sha256_hash_batch(inBuffer, inlen, outBuffer, (SHA_WORD)n_batch);
		auto batchEndTime = std::chrono::steady_clock::now();
		double batchTimeMs = std::chrono::duration<double, std::milli>(batchEndTime - batchStartTime).count();
 
		if (cpuErr != 0) {
			break;
		}
 
		// Time the single-hash on CPU
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
 
		double rate = (batchTimeMs > 0.0) ? (n_batch / (batchTimeMs / 1000.0)) : 0.0;
		RunMeasurement measurement = { r, singleTimeMs, batchTimeMs, batchTimeMs, rate };
		runsList.push_back(measurement);
 
		totalBatchTimeMs += batchTimeMs;
		totalSingleTimeMs += singleTimeMs;
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
	size_t totalDataProcessed = (size_t)(inlen + 4) * n_batch * numberOfRuns;
	GPUMetrics result = {};
	result.valid = true;
	result.batchSize = n_batch;
	result.sampleHashes = sampleHashes;
	result.hashResult = sampleHashes.empty() ? "" : sampleHashes[0].second;
	result.singleHashTimeMs = totalSingleTimeMs / numberOfRuns;
	result.batchHashTimeMs = totalBatchTimeMs / numberOfRuns;
	result.executionTime = totalBatchTimeMs / 1000.0; // Convert to seconds
	result.throughput = (totalDataProcessed > 0 && totalBatchTimeMs > 0.0) 
		? (totalDataProcessed / (1024.0 * 1024.0)) / (totalBatchTimeMs / 1000.0)
		: 0.0;
	result.avgPowerDraw = 0.0; // CPU power not measured
	result.totalEnergy = 0.0;  // CPU energy not measured
	result.runs = runsList;
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
