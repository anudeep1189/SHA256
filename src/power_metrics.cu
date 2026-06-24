/*
 * power_metrics.cu - Power Measurement and Timing Implementation
 * Single Responsibility: NVML power sampling + CUDA event timing + metric computation
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#include "power_metrics.h"
#include <chrono>
#include <numeric>
#include <sstream>
#include <iomanip>

PowerMetrics::PowerMetrics()
	: nvmlInitialized(false)
	, nvmlDevice(nullptr)
	, sampling(false)
	, elapsedMs(0.0f)
	, timerStart(nullptr)
	, timerStop(nullptr)
{
}

PowerMetrics::~PowerMetrics()
{
	if (sampling.load()) {
		stopSampling();
	}
	if (timerStart) cudaEventDestroy(timerStart);
	if (timerStop) cudaEventDestroy(timerStop);
	shutdown();
}

bool PowerMetrics::init()
{
	nvmlReturn_t result = nvmlInit_v2();
	if (result != NVML_SUCCESS) {
		nvmlInitialized = false;
		return false;
	}

	result = nvmlDeviceGetHandleByIndex(0, &nvmlDevice);
	if (result != NVML_SUCCESS) {
		nvmlShutdown();
		nvmlInitialized = false;
		return false;
	}

	nvmlInitialized = true;

	// Create CUDA timing events
	cudaEventCreate(&timerStart);
	cudaEventCreate(&timerStop);

	return true;
}

void PowerMetrics::shutdown()
{
	if (nvmlInitialized) {
		nvmlShutdown();
		nvmlInitialized = false;
	}
}

bool PowerMetrics::isAvailable() const
{
	return nvmlInitialized;
}

void PowerMetrics::startSampling()
{
	if (!nvmlInitialized) return;

	{
		std::lock_guard<std::mutex> lock(samplesMutex);
		powerSamples.clear();
	}

	sampling.store(true);
	samplingThread = std::thread(&PowerMetrics::samplingLoop, this);
}

void PowerMetrics::stopSampling()
{
	sampling.store(false);
	if (samplingThread.joinable()) {
		samplingThread.join();
	}
}

void PowerMetrics::samplingLoop()
{
	while (sampling.load()) {
		unsigned int power = 0;  // milliwatts
		nvmlReturn_t result = nvmlDeviceGetPowerUsage(nvmlDevice, &power);
		if (result == NVML_SUCCESS) {
			std::lock_guard<std::mutex> lock(samplesMutex);
			powerSamples.push_back(power);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

double PowerMetrics::getAveragePower() const
{
	std::lock_guard<std::mutex> lock(samplesMutex);
	if (powerSamples.empty()) return 0.0;

	unsigned long long sum = 0;
	for (auto s : powerSamples) {
		sum += s;
	}
	// NVML returns milliwatts, convert to watts
	return (double)sum / powerSamples.size() / 1000.0;
}

void PowerMetrics::startTimer()
{
	elapsedMs = 0.0f;
	cudaEventRecord(timerStart, 0);
}

void PowerMetrics::stopTimer()
{
	cudaEventRecord(timerStop, 0);
	cudaEventSynchronize(timerStop);
	cudaEventElapsedTime(&elapsedMs, timerStart, timerStop);
}

double PowerMetrics::getElapsedTime() const
{
	// Convert milliseconds to seconds
	return (double)elapsedMs / 1000.0;
}

double PowerMetrics::getEnergy() const
{
	// Energy (Joules) = Power (Watts) * Time (Seconds)
	return getAveragePower() * getElapsedTime();
}

double PowerMetrics::getThroughput(size_t dataSize) const
{
	double timeSec = getElapsedTime();
	if (timeSec <= 0.0) return 0.0;
	// MB/s = bytes / seconds / (1024*1024)
	return (double)dataSize / timeSec / (1024.0 * 1024.0);
}

GPUMetrics PowerMetrics::buildMetrics(size_t dataSize, const std::string& hashHex) const
{
	GPUMetrics metrics;
	metrics.executionTime = getElapsedTime();
	metrics.avgPowerDraw = getAveragePower();
	metrics.totalEnergy = getEnergy();
	metrics.throughput = getThroughput(dataSize);
	metrics.hashResult = hashHex;
	metrics.valid = true;
	metrics.errorMessage = "";
	return metrics;
}
