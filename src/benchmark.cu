#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <cuda_runtime.h>
#include "sha256_cpu.h"

extern "C" {
cudaError_t mcm_cuda_sha256_hash_batch(BYTE* in, SHA_WORD inlen, BYTE* out, SHA_WORD n_batch);
}

int main()
{
	const int RUNS = 20;
	const SHA_WORD BATCH_SIZE = 1000000;
	const std::string text = "abc";
	const SHA_WORD inlen = text.size();

	std::cout << "=========================================================\n";
	std::cout << "          SHA256 GPU & CPU Benchmark Tool\n";
	std::cout << "=========================================================\n";
	std::cout << "Hashing text: \"" << text << "\"\n";
	std::cout << "Batch size  : " << BATCH_SIZE << " iterations\n";
	std::cout << "Number of runs: " << RUNS << "\n";

	std::ofstream out_file("benchmark_results.txt");
	if (!out_file.is_open()) {
		std::cerr << "Error: Failed to open benchmark_results.txt for writing.\n";
		return 1;
	}

	out_file << "Run,GPU_Time_ms,CPU_Time_ms\n";
	std::cout << "Writing results to benchmark_results.txt...\n\n";

	// Prepare input data
	std::vector<BYTE> inputData(text.begin(), text.end());

	// Prepare output buffers
	size_t totalOutputSize = (size_t)32 * BATCH_SIZE;
	std::vector<BYTE> gpuOut(totalOutputSize);
	std::vector<BYTE> cpuOut(totalOutputSize);

	// Warm up the CUDA context to hide first-run latency during the benchmark
	std::cout << "Warming up CUDA context...\n";
	cudaFree(0);
	std::cout << "Warmup complete. Starting runs...\n\n";

	// Run 20 iterations
	for (int run = 1; run <= RUNS; ++run) {
		std::cout << "Executing Run " << run << "/" << RUNS << "... " << std::flush;

		// 1. GPU Hashing Time
		cudaEvent_t start, stop;
		cudaEventCreate(&start);
		cudaEventCreate(&stop);

		cudaEventRecord(start);
		cudaError_t gpuErr = mcm_cuda_sha256_hash_batch(inputData.data(), inlen, gpuOut.data(), BATCH_SIZE);
		cudaEventRecord(stop);
		cudaEventSynchronize(stop);

		float gpuMs = 0.0f;
		cudaEventElapsedTime(&gpuMs, start, stop);
		cudaEventDestroy(start);
		cudaEventDestroy(stop);

		if (gpuErr != cudaSuccess) {
			std::cerr << "\nGPU Hashing failed on run " << run << " with error: " << cudaGetErrorString(gpuErr) << "\n";
			out_file.close();
			return 1;
		}

		// 2. CPU Hashing Time
		auto cpuStart = std::chrono::high_resolution_clock::now();
		int cpuErr = mcm_cpu_sha256_hash_batch(inputData.data(), inlen, cpuOut.data(), BATCH_SIZE);
		auto cpuStop = std::chrono::high_resolution_clock::now();
		double cpuMs = std::chrono::duration<double, std::milli>(cpuStop - cpuStart).count();

		if (cpuErr != 0) {
			std::cerr << "\nCPU Hashing failed on run " << run << "\n";
			out_file.close();
			return 1;
		}

		// Log to console and file
		std::cout << "GPU: " << gpuMs << " ms | CPU: " << cpuMs << " ms\n";
		out_file << run << "," << gpuMs << "," << cpuMs << "\n";
	}

	out_file.close();
	std::cout << "\nBenchmark complete! Results saved to benchmark_results.txt\n";
	return 0;
}
