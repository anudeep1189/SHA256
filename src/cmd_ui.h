/*
 * cmd_ui.h - FTXUI Terminal UI Interface
 * Single Responsibility: Render a terminal dashboard interface using FTXUI.
 *
 * Part of SHA256 CUDA Hasher Phase 2
 */

#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include "gpu_device_info.h"
#include "power_metrics.h"

// UI Event types for callback
enum class UIEvent {
	RUN_CLICKED,
	RUN_CLICKED_CPU,
	CLEAR_CLICKED,
	QUIT
};

// Callback type for UI events
typedef std::function<void(UIEvent)> UIEventCallback;

class CmdUI {
public:
	CmdUI();
	~CmdUI();

	// Initialize TUI settings (no window handles needed)
	void initialize(int width = 0, int height = 0);

	// Redraw/status compatibility helpers
	void drawFrame();
	void drawResultsPanel(const GPUMetrics& metrics, bool isGPU);
	void drawStatusMessage(const std::string& msg, bool isGPU, bool isError = false);
	void clearResults();
	void clearAll();

	// TUI event loop - blocks until terminal screen quits
	void runEventLoop(UIEventCallback callback);

	// Getters for current UI state
	std::string getTextInput() const;
	int getBatchSize() const;
	int getRunsCount() const;

	// Set GPU info for display
	void setGPUInfo(const GPUDeviceInfo& info);

private:
	// UI State input fields (bound to FTXUI Input components)
	std::string textInputStr;
	std::string batchSizeStr;
	std::string runsCountStr;
	GPUDeviceInfo gpuInfo;

	// Results metrics
	GPUMetrics gpuMetrics;
	GPUMetrics cpuMetrics;
	bool hasGpuMetrics;
	bool hasCpuMetrics;

	// Status messages
	std::string gpuStatusMsg;
	std::string cpuStatusMsg;
	bool gpuStatusIsError;
	bool cpuStatusIsError;

	// Running states
	std::atomic<bool> isGpuRunning;
	std::atomic<bool> isCpuRunning;

	// Screen refresh trigger
	std::function<void()> redrawTrigger;

	// Mutex for synchronizing worker thread data
	std::mutex metricsMutex;

	// Scroll offsets for results tables
	int gpuScrollOffset;
	int cpuScrollOffset;

	// Last mouse cursor X coordinate
	int lastMouseX;
};
