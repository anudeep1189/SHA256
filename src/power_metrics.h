/*
 * power_metrics.h - Power Measurement and Timing Interface
 * Single Responsibility: NVML power sampling + CUDA event timing + metric computation
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#pragma once

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <cuda_runtime.h>
#include <nvml.h>

struct GPUMetrics {
	double executionTime;    // seconds
	double avgPowerDraw;     // watts
	double totalEnergy;      // joules
	double throughput;       // MB/s
	std::string hashResult;  // SHA256 hex string (first hash)
	int batchSize;           // number of hashes computed
	std::vector<std::pair<int, std::string>> sampleHashes; // index + hex pairs
	double singleHashTimeMs; // kernel time for batch=1
	double batchHashTimeMs;  // kernel time for full batch
	bool valid;
	std::string errorMessage;
};

class PowerMetrics {
public:
	PowerMetrics();
	~PowerMetrics();

	// NVML lifecycle
	bool init();
	void shutdown();
	bool isAvailable() const;

	// Power sampling (background thread)
	void startSampling();
	void stopSampling();
	double getAveragePower() const;    // watts

	// CUDA event timing
	void startTimer();
	void stopTimer();
	double getElapsedTime() const;     // seconds

	// Computed metrics
	double getEnergy() const;          // joules = avgPower * time
	double getThroughput(size_t dataSize) const;  // MB/s = dataSize / time

	// Build complete metrics struct
	GPUMetrics buildMetrics(size_t dataSize, const std::string& hashHex) const;

private:
	// NVML state
	bool nvmlInitialized;
	nvmlDevice_t nvmlDevice;

	// Power sampling
	std::thread samplingThread;
	std::atomic<bool> sampling;
	mutable std::mutex samplesMutex;
	std::vector<unsigned int> powerSamples;  // milliwatts from NVML

	// CUDA timing
	cudaEvent_t timerStart;
	cudaEvent_t timerStop;
	float elapsedMs;

	// Internal sampling loop
	void samplingLoop();
};
