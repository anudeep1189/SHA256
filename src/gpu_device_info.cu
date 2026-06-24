/*
 * gpu_device_info.cu - GPU Device Discovery Implementation
 * Single Responsibility: Query and validate GPU hardware capabilities
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#include "gpu_device_info.h"
#include <cuda_runtime.h>

int getCoresPerSM(int major, int minor)
{
	// Architecture lookup table for CUDA cores per Streaming Multiprocessor
	// Based on NVIDIA GPU architecture specifications
	switch (major) {
		case 2: // Fermi
			return (minor == 1) ? 48 : 32;
		case 3: // Kepler
			return 192;
		case 5: // Maxwell
			return 128;
		case 6: // Pascal
			return (minor == 0) ? 64 : 128;
		case 7: // Volta / Turing
			return (minor == 0) ? 64 : 64;
		case 8: // Ampere / Ada Lovelace
			return (minor == 0) ? 64 : 128;
		case 9: // Hopper
			return 128;
		default:
			return 128; // Default fallback for future architectures
	}
}

GPUDeviceInfo queryGPUDevice()
{
	GPUDeviceInfo info = {};
	info.valid = false;

	int deviceCount = 0;
	cudaError_t err = cudaGetDeviceCount(&deviceCount);
	if (err != cudaSuccess || deviceCount == 0) {
		info.errorMessage = "No CUDA-capable GPU detected.";
		return info;
	}

	cudaDeviceProp prop;
	err = cudaGetDeviceProperties(&prop, 0);
	if (err != cudaSuccess) {
		info.errorMessage = "Failed to query GPU device properties.";
		return info;
	}

	info.deviceName = prop.name;
	info.multiProcessorCount = prop.multiProcessorCount;
	info.computeCapabilityMajor = prop.major;
	info.computeCapabilityMinor = prop.minor;
	info.maxThreadsPerBlock = prop.maxThreadsPerBlock;
	info.totalMemory = prop.totalGlobalMem;

	// Calculate CUDA core count
	int coresPerSM = getCoresPerSM(prop.major, prop.minor);
	info.cudaCoreCount = prop.multiProcessorCount * coresPerSM;

	// Max total threads = multiprocessors * max threads per multiprocessor
	info.maxTotalThreads = prop.multiProcessorCount * prop.maxThreadsPerMultiProcessor;

	// Query free memory
	size_t freeMem = 0, totalMem = 0;
	cudaMemGetInfo(&freeMem, &totalMem);
	info.freeMemory = freeMem;

	info.valid = true;
	return info;
}

bool validateBatchCount(const GPUDeviceInfo& info, int n_batch, size_t inputLen, std::string& errorMessage)
{
	if (n_batch <= 0) {
		errorMessage = "Thread count must be greater than 0.";
		return false;
	}

	if (!info.valid) {
		errorMessage = "No valid GPU device available.";
		return false;
	}

	// Each batch item needs: inputLen bytes (input) + 32 bytes (output) on GPU
	size_t requiredMemory = (size_t)n_batch * (inputLen + 32);

	// Use 80% of free memory as safe limit
	size_t safeLimit = (size_t)(info.freeMemory * 0.8);

	if (requiredMemory > safeLimit) {
		errorMessage = "Requested batch count exceeds available GPU memory. Max safe batch: "
					 + std::to_string((int)(safeLimit / (inputLen + 32)));
		return false;
	}

	return true;
}
