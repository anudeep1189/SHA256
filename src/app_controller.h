/*
 * app_controller.h - Application Controller Interface
 * Single Responsibility: Orchestrate UI, input, GPU kernel execution, and metrics collection
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#pragma once

#include "cmd_ui.h"
#include "input_handler.h"
#include "gpu_device_info.h"
#include "power_metrics.h"
#include <string>
#include <vector>

class AppController {
public:
	AppController(CmdUI& ui, InputHandler& inputHandler,
				  GPUDeviceInfo& gpuInfo, PowerMetrics& metrics);
	~AppController();

	// Main application loop
	void run();

private:
	// Component references (dependency injection)
	CmdUI& ui;
	InputHandler& inputHandler;
	GPUDeviceInfo& gpuInfo;
	PowerMetrics& metrics;

	// Event handler
	void onUIEvent(UIEvent event);

	// Core execution
	void onRunHashGPU();

	// Helper: convert 32-byte hash to hex string
	std::string hashToHex(const unsigned char* hash, int len);

	// Running state
	bool running;
};
