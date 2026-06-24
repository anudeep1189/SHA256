/*
 * cmd_ui.h - Console UI Interface
 * Single Responsibility: Render clickable/scrollable CMD interface and handle user input events
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "gpu_device_info.h"
#include "power_metrics.h"

// Clickable region definition
struct ClickRegion {
	SMALL_RECT bounds;
	int id;
	std::string label;
};

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

	// Initialize console with desired dimensions
	void initialize(int width = 110, int height = 42);

	// Draw the complete UI frame
	void drawFrame();

	// Draw individual sections
	void drawTitle();
	void drawTextboxes();
	void drawGPUInfo(const GPUDeviceInfo& info);
	void drawBatchInput();
	void drawRunButtons();
	void drawRunCPUButton();
	void drawClearButton();
	void drawResultsPanel(const GPUMetrics& metrics, bool isGPU);
	void drawStatusMessage(const std::string& msg, bool isGPU, bool isError = false);
	void drawStatusBar();

	// Clear results area
	void clearResults();

	// Clear input and results
	void clearAll();

	// Event loop - returns when user quits
	void runEventLoop(UIEventCallback callback);

	// Getters for current UI state
	std::string getTextInput() const;
	int getBatchSize() const;

	// Set GPU info for display
	void setGPUInfo(const GPUDeviceInfo& info);

private:
	// Console handles
	HANDLE hOut;
	HANDLE hIn;
	int consoleWidth;
	int consoleHeight;

	// UI State
	std::string textInput;
	std::string batchSizeStr;
	GPUDeviceInfo gpuInfo;

	// Clickable regions
	std::vector<ClickRegion> clickRegions;

	// Active textbox (0=text, 1=batch)
	int activeTextbox;

	// Results scroll state
	int resultsScrollOffset;
	std::vector<std::string> resultsLines;

	// Last compute mode ("GPU" or "CPU") for results title
	std::string lastComputeMode;

	// Side-by-side results metrics
	GPUMetrics cpuMetrics;
	GPUMetrics gpuMetrics;
	bool hasCpuMetrics;
	bool hasGpuMetrics;

	// Helper for drawing a column of results
	void drawResultColumn(const GPUMetrics& metrics, int colStart, bool isGPU);

	// Update consoleWidth and consoleHeight dynamically based on the actual screen buffer info
	void updateDimensions();

	// Color helpers
	void setColor(WORD color);
	void resetColor();
	void setCursor(int x, int y);
	void clearLine(int y, int startX, int endX);
	void drawBox(int x, int y, int width, int height);
	void drawHorizontalLine(int y, int startX, int endX);

	// Event handling
	void handleMouseClick(int x, int y, UIEventCallback& callback);
	void handleKeyPress(KEY_EVENT_RECORD keyEvent, UIEventCallback& callback);
	void handleScrollWheel(int delta);

	// Region management
	void registerClickRegion(int x, int y, int width, int height, int id, const std::string& label);
	int hitTestRegion(int x, int y);

	// Textbox rendering helper
	void drawSingleTextbox(int row, const std::string& label, const std::string& value, bool active, int textboxId);

	// Clipboard helpers
	void copyToClipboard(const std::string& text);
	std::string pasteFromClipboard();

	// Constants
	static const int ROW_TITLE = 1;
	static const int ROW_TEXT_INPUT = 4;
	static const int ROW_GPU_INFO = 6;
	static const int ROW_BATCH = 8;
	static const int ROW_BUTTON = 10;
	static const int ROW_RESULTS_START = 12;
	static const int COL_START = 2;
	static const int COL_INPUT_START = 16;

	// Region IDs
	static const int REGION_RUN_BUTTON = 3;
	static const int REGION_RUN_BUTTON_CPU = 7;
	static const int REGION_CLEAR_BUTTON = 5;
	static const int REGION_TEXTBOX_TEXT = 4;
	static const int REGION_TEXTBOX_BATCH = 6;

	// Color scheme
	static const WORD COLOR_BORDER = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	static const WORD COLOR_TITLE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	static const WORD COLOR_LABEL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	static const WORD COLOR_INPUT_ACTIVE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
	static const WORD COLOR_INPUT_INACTIVE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_BLUE;
	static const WORD COLOR_BUTTON = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	static const WORD COLOR_BUTTON_CLEAR = FOREGROUND_RED | FOREGROUND_INTENSITY;
	static const WORD COLOR_RESULT_LABEL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	static const WORD COLOR_RESULT_VALUE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
	static const WORD COLOR_HASH = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	static const WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;
	static const WORD COLOR_STATUS = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
	static const WORD COLOR_STATUSBAR = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_BLUE;
	static const WORD COLOR_SECTION_HEADER = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
};
