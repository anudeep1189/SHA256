/*
 * cmd_ui.cpp - FTXUI Terminal UI Implementation
 * Single Responsibility: Compose reactive terminal layouts, handle events, and draw inputs/results.
 *
 * Part of SHA256 CUDA Hasher Phase 2
 */

#include "cmd_ui.h"
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/terminal.hpp>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
void disableWindowsCtrlCControlEvent()
{
	HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	if (stdinHandle == INVALID_HANDLE_VALUE) {
		return;
	}

	DWORD mode = 0;
	if (!GetConsoleMode(stdinHandle, &mode)) {
		return;
	}

	mode &= ~ENABLE_PROCESSED_INPUT;
	SetConsoleMode(stdinHandle, mode);
}
}
#endif

CmdUI::CmdUI()
	: textInputStr("The quick brown fox jumps over the lazy dog")
	, batchSizeStr("100000")
	, runsCountStr("100")
	, hasGpuMetrics(false)
	, hasCpuMetrics(false)
	, gpuMetrics{}
	, cpuMetrics{}
	, gpuStatusIsError(false)
	, cpuStatusIsError(false)
	, isGpuRunning(false)
	, isCpuRunning(false)
	, redrawTrigger(nullptr)
	, gpuScrollOffset(0)
	, cpuScrollOffset(0)
	, lastMouseX(0)
{
}

CmdUI::~CmdUI()
{
}

void CmdUI::initialize(int width, int height)
{
	// No-op for FTXUI initialization
}

void CmdUI::drawFrame()
{
	if (redrawTrigger) {
		redrawTrigger();
	}
}

void CmdUI::drawResultsPanel(const GPUMetrics& metrics, bool isGPU)
{
	{
		std::lock_guard<std::mutex> lock(metricsMutex);
		if (isGPU) {
			gpuMetrics = metrics;
			hasGpuMetrics = true;
			gpuStatusMsg = "";
			gpuScrollOffset = 0;  // Always start at the top when new results arrive
		} else {
			cpuMetrics = metrics;
			hasCpuMetrics = true;
			cpuStatusMsg = "";
			cpuScrollOffset = 0;  // Always start at the top when new results arrive
		}
	}
	if (redrawTrigger) {
		redrawTrigger();
	}
}

void CmdUI::drawStatusMessage(const std::string& msg, bool isGPU, bool isError)
{
	{
		std::lock_guard<std::mutex> lock(metricsMutex);
		if (isGPU) {
			gpuStatusMsg = msg;
			gpuStatusIsError = isError;
		} else {
			cpuStatusMsg = msg;
			cpuStatusIsError = isError;
		}
	}
	if (redrawTrigger) {
		redrawTrigger();
	}
}

void CmdUI::clearResults()
{
	{
		std::lock_guard<std::mutex> lock(metricsMutex);
		hasGpuMetrics = false;
		hasCpuMetrics = false;
		gpuStatusMsg = "";
		cpuStatusMsg = "";
		gpuScrollOffset = 0;
		cpuScrollOffset = 0;
	}
	if (redrawTrigger) {
		redrawTrigger();
	}
}

void CmdUI::clearAll()
{
	{
		std::lock_guard<std::mutex> lock(metricsMutex);
		hasGpuMetrics = false;
		hasCpuMetrics = false;
		gpuStatusMsg = "";
		cpuStatusMsg = "";
		textInputStr = "The quick brown fox jumps over the lazy dog";
		batchSizeStr = "100000";
		runsCountStr = "100";
		gpuScrollOffset = 0;
		cpuScrollOffset = 0;
	}
	if (redrawTrigger) {
		redrawTrigger();
	}
}

std::string CmdUI::getTextInput() const
{
	return textInputStr;
}

int CmdUI::getBatchSize() const
{
	try {
		return std::stoi(batchSizeStr);
	} catch (...) {
		return 100000;
	}
}

int CmdUI::getRunsCount() const
{
	try {
		return std::stoi(runsCountStr);
	} catch (...) {
		return 10;
	}
}

void CmdUI::setGPUInfo(const GPUDeviceInfo& info)
{
	gpuInfo = info;
}

void CmdUI::runEventLoop(UIEventCallback callback)
{
	using namespace ftxui;

	auto screen = ScreenInteractive::TerminalOutput();
	screen.ForceHandleCtrlC(false);
#if defined(_WIN32)
	screen.Post(disableWindowsCtrlCControlEvent);
#endif
	redrawTrigger = [&]() { screen.PostEvent(Event::Custom); };

	// Setup inputs
	Component input_text = Input(&textInputStr, "The quick brown fox jumps over the lazy dog");
	Component input_batch = Input(&batchSizeStr, "100000");
	Component input_runs = Input(&runsCountStr, "100");
	constexpr int kVisibleRunRows = 7;

	// Setup buttons
	Component btn_gpu = Button("Run Hash (GPU)", [&]() {
		if (isGpuRunning || isCpuRunning) return;
		isGpuRunning = true;
		gpuScrollOffset = 0;
		drawStatusMessage("Processing GPU Hashing...", true, false);
		std::thread([this, callback]() {
			callback(UIEvent::RUN_CLICKED);
			isGpuRunning = false;
			if (redrawTrigger) redrawTrigger();
		}).detach();
	});

	Component btn_cpu = Button("Run Hash (CPU)", [&]() {
		if (isGpuRunning || isCpuRunning) return;
		isCpuRunning = true;
		cpuScrollOffset = 0;
		drawStatusMessage("Processing CPU Hashing...", false, false);
		std::thread([this, callback]() {
			callback(UIEvent::RUN_CLICKED_CPU);
			isCpuRunning = false;
			if (redrawTrigger) redrawTrigger();
		}).detach();
	});

	Component btn_clear = Button("Clear Results", [&]() {
		if (isGpuRunning || isCpuRunning) return;
		callback(UIEvent::CLEAR_CLICKED);
	});

	Component btn_quit = Button("Quit Application", [&]() {
		screen.Exit();
		callback(UIEvent::QUIT);
	});

	// Nest components
	auto input_container = Container::Vertical({
		input_text,
		input_batch,
		input_runs,
	});

	auto button_container = Container::Horizontal({
		btn_gpu,
		btn_cpu,
		btn_clear,
		btn_quit,
	});

	auto main_container = Container::Vertical({
		input_container,
		button_container,
	});

	auto renderer = Renderer(main_container, [&] {
		// Acquire mutex lock while rendering TUI layouts
		std::lock_guard<std::mutex> lock(metricsMutex);

		// --- Best Practice: Guard against terminal being too small ---
		auto termSize = Terminal::Size();
		int termW = termSize.dimx;
		int termH = termSize.dimy;
		const int MIN_W = 130;
		const int MIN_H = 30;
		if (termW < MIN_W || termH < MIN_H) {
			return vbox({
				text(" "),
				text("  Terminal too small!") | bold | color(Color::Red),
				text(" "),
				text("  Please resize the window to at least:") | color(Color::Yellow),
				text("    Width : " + std::to_string(MIN_W) + " chars  (current: " + std::to_string(termW) + ")") | color(Color::Yellow),
				text("    Height: " + std::to_string(MIN_H) + " rows   (current: " + std::to_string(termH) + ")") | color(Color::Yellow),
			});
		}

		bool disableFields = isGpuRunning || isCpuRunning;
		Element input_text_el = disableFields ? text(textInputStr) | dim : input_text->Render();
		Element input_batch_el = disableFields ? text(batchSizeStr) | dim : input_batch->Render();
		Element input_runs_el = disableFields ? text(runsCountStr) | dim : input_runs->Render();

		auto parameters_box = window(text("1. Benchmark Parameters") | bold | color(Color::Cyan), vbox({
			hbox(text("Text to Hash:   ") | color(Color::Green), input_text_el),
			hbox(text("Batch Size:     ") | color(Color::Green), input_batch_el),
			hbox(text("Number of Runs: ") | color(Color::Green), input_runs_el),
			separator(),
			text("Validation Limits:") | bold | color(Color::Yellow),
			text("  • Max GPU Batch Size: 50,000,000") | color(Color::Yellow),
			text("  • Max CPU Batch Size: 10,000,000") | color(Color::Yellow),
		}));

		auto execution_box = window(text("2. Execution Control") | bold | color(Color::Cyan), vbox({
			text("GPU Model: " + (gpuInfo.valid ? gpuInfo.deviceName : "N/A (NVML not initialized/found)")) | color(Color::Yellow),
			separator(),
			hbox({
				disableFields ? (isGpuRunning ? text("[ Running GPU... ]") | bold | color(Color::Yellow) : text("[ Run Hash (GPU) ]") | dim) : btn_gpu->Render(),
				text("  "),
				disableFields ? (isCpuRunning ? text("[ Running CPU... ]") | bold | color(Color::Yellow) : text("[ Run Hash (CPU) ]") | dim) : btn_cpu->Render(),
				text("  "),
				disableFields ? text("[ Clear Results ]") | dim : btn_clear->Render(),
				text("  "),
				btn_quit->Render(),
			}),
		}));

		// Local hash formatter
		auto formatHashRateLocal = [](double rate) {
			if (rate >= 1e9) return std::to_string(rate / 1e9).substr(0, 5) + " GH/s";
			if (rate >= 1e6) return std::to_string(rate / 1e6).substr(0, 5) + " MH/s";
			if (rate >= 1e3) return std::to_string(rate / 1e3).substr(0, 5) + " KH/s";
			return std::to_string(rate).substr(0, 5) + " H/s";
		};
 
		// Custom scrollbar drawer
		auto drawScrollbar = [](int totalItems, int startIndex, int visibleCount) {
			if (totalItems <= visibleCount) return vbox(); // No scrollbar needed
			
			std::vector<Element> bar;
			int height = visibleCount + 1; // Header + visible data rows
			double fractionVisible = (double)visibleCount / totalItems;
			double fractionScrolled = (double)startIndex / (totalItems - visibleCount);
			
			int thumbHeight = std::max(1, (int)(height * fractionVisible));
			int maxScrollRange = height - thumbHeight;
			int thumbStart = (int)(maxScrollRange * fractionScrolled);
			
			for (int i = 0; i < height; ++i) {
				if (i >= thumbStart && i < thumbStart + thumbHeight) {
					bar.push_back(text("█") | color(Color::Cyan));
				} else {
					bar.push_back(text("░") | color(Color::GrayDark));
				}
			}
			return vbox(bar);
		};

		// Left Column: GPU Results
		Element gpu_panel;
		if (isGpuRunning) {
			gpu_panel = vbox({
				text("RESULTS - GPU") | bold | color(Color::Green),
				separator(),
				text("Processing GPU Hashing... Please wait.") | bold | color(Color::Yellow),
			});
		} else if (hasGpuMetrics && gpuMetrics.valid) {
			std::vector<std::vector<std::string>> tData;
			tData.push_back({"Run", "Single (ms)", "Batch (ms)", "Runtime (ms)", "Rate"});
			
			int totalRuns = (int)gpuMetrics.runs.size();
			int visibleCount = std::min(totalRuns, kVisibleRunRows);  // Keep compact so summary always fits
			int startIndex = std::min(gpuScrollOffset, std::max(0, totalRuns - visibleCount));
			
			for (int i = startIndex; i < std::min(totalRuns, startIndex + visibleCount); ++i) {
				const auto& r = gpuMetrics.runs[i];
				tData.push_back({
					std::to_string(r.runNumber),
					std::to_string(r.singleMs).substr(0, 7),
					std::to_string(r.batchMs).substr(0, 7),
					std::to_string(r.runtimeMs).substr(0, 7),
					formatHashRateLocal(r.rate)
				});
			}
			Table t(tData);
			t.SelectAll().Border(LIGHT);
			t.SelectAll().Separator(LIGHT);
			t.SelectRow(0).Decorate(bold);
			t.SelectColumn(0).DecorateCells(size(WIDTH, GREATER_THAN, 4));
			t.SelectColumn(1).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			t.SelectColumn(2).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			t.SelectColumn(3).DecorateCells(size(WIDTH, GREATER_THAN, 11));
			t.SelectColumn(4).DecorateCells(size(WIDTH, GREATER_THAN, 10));

			double avgSingle = 0, avgBatch = 0, avgRuntime = 0, avgRate = 0, totalRuntime = 0;
			if (!gpuMetrics.runs.empty()) {
				for (const auto& r : gpuMetrics.runs) {
					avgSingle += r.singleMs; avgBatch += r.batchMs; avgRuntime += r.runtimeMs; avgRate += r.rate; totalRuntime += r.runtimeMs;
				}
				avgSingle /= gpuMetrics.runs.size(); avgBatch /= gpuMetrics.runs.size(); avgRuntime /= gpuMetrics.runs.size(); avgRate /= gpuMetrics.runs.size();
			}
			std::vector<std::vector<std::string>> sumData;
			sumData.push_back({"Metric", "Single (ms)", "Batch (ms)", "Runtime (ms)", "Rate"});
			sumData.push_back({
				"Average",
				std::to_string(avgSingle).substr(0, 7),
				std::to_string(avgBatch).substr(0, 7),
				std::to_string(avgRuntime).substr(0, 7),
				formatHashRateLocal(avgRate)
			});
			sumData.push_back({
				"Total Time",
				"-",
				"-",
				std::to_string(totalRuntime).substr(0, 7) + " ms",
				"-"
			});
			Table sum_t(sumData);
			sum_t.SelectAll().Border(LIGHT);
			sum_t.SelectAll().Separator(LIGHT);
			sum_t.SelectRow(0).Decorate(bold);
			sum_t.SelectRow(2).Decorate(bold);
			sum_t.SelectColumn(0).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(1).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(2).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(3).DecorateCells(size(WIDTH, GREATER_THAN, 11));
			sum_t.SelectColumn(4).DecorateCells(size(WIDTH, GREATER_THAN, 10));

			std::string runDetailsLabel = "Run Details:";

			gpu_panel = vbox({
				text("RESULTS - GPU") | bold | color(Color::Green),
				separator(),
				text("Hashes Computed: " + std::to_string(gpuMetrics.batchSize)),
				text("Original Hash:   " + gpuMetrics.hashResult) | color(Color::Green),
				text(runDetailsLabel) | bold,
				hbox({ t.Render(), drawScrollbar(totalRuns, startIndex, visibleCount) }),
				separator(),
				text("Summary:") | bold,
				sum_t.Render(),
			});
		} else {
			gpu_panel = vbox({
				text("RESULTS - GPU") | bold | color(Color::Green),
				separator(),
				text("No GPU metrics recorded yet.") | dim,
			});
		}

		// Right Column: CPU Results
		Element cpu_panel;
		if (isCpuRunning) {
			cpu_panel = vbox({
				text("RESULTS - CPU") | bold | color(Color::Yellow),
				separator(),
				text("Processing CPU Hashing... Please wait.") | bold | color(Color::Yellow),
			});
		} else if (hasCpuMetrics && cpuMetrics.valid) {
			std::vector<std::vector<std::string>> tData;
			tData.push_back({"Run", "Single (ms)", "Batch (ms)", "Runtime (ms)", "Rate"});
			
			int totalRuns = (int)cpuMetrics.runs.size();
			int visibleCount = std::min(totalRuns, kVisibleRunRows);  // Keep compact so summary always fits
			int startIndex = std::min(cpuScrollOffset, std::max(0, totalRuns - visibleCount));
			
			for (int i = startIndex; i < std::min(totalRuns, startIndex + visibleCount); ++i) {
				const auto& r = cpuMetrics.runs[i];
				tData.push_back({
					std::to_string(r.runNumber),
					std::to_string(r.singleMs).substr(0, 7),
					std::to_string(r.batchMs).substr(0, 7),
					std::to_string(r.runtimeMs).substr(0, 7),
					formatHashRateLocal(r.rate)
				});
			}
			Table t(tData);
			t.SelectAll().Border(LIGHT);
			t.SelectAll().Separator(LIGHT);
			t.SelectRow(0).Decorate(bold);
			t.SelectColumn(0).DecorateCells(size(WIDTH, GREATER_THAN, 4));
			t.SelectColumn(1).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			t.SelectColumn(2).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			t.SelectColumn(3).DecorateCells(size(WIDTH, GREATER_THAN, 11));
			t.SelectColumn(4).DecorateCells(size(WIDTH, GREATER_THAN, 10));

			double avgSingle = 0, avgBatch = 0, avgRuntime = 0, avgRate = 0, totalRuntime = 0;
			if (!cpuMetrics.runs.empty()) {
				for (const auto& r : cpuMetrics.runs) {
					avgSingle += r.singleMs; avgBatch += r.batchMs; avgRuntime += r.runtimeMs; avgRate += r.rate; totalRuntime += r.runtimeMs;
				}
				avgSingle /= cpuMetrics.runs.size(); avgBatch /= cpuMetrics.runs.size(); avgRuntime /= cpuMetrics.runs.size(); avgRate /= cpuMetrics.runs.size();
			}
			std::vector<std::vector<std::string>> sumData;
			sumData.push_back({"Metric", "Single (ms)", "Batch (ms)", "Runtime (ms)", "Rate"});
			sumData.push_back({
				"Average",
				std::to_string(avgSingle).substr(0, 7),
				std::to_string(avgBatch).substr(0, 7),
				std::to_string(avgRuntime).substr(0, 7),
				formatHashRateLocal(avgRate)
			});
			sumData.push_back({
				"Total Time",
				"-",
				"-",
				std::to_string(totalRuntime).substr(0, 7) + " ms",
				"-"
			});
			Table sum_t(sumData);
			sum_t.SelectAll().Border(LIGHT);
			sum_t.SelectAll().Separator(LIGHT);
			sum_t.SelectRow(0).Decorate(bold);
			sum_t.SelectRow(2).Decorate(bold);
			sum_t.SelectColumn(0).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(1).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(2).DecorateCells(size(WIDTH, GREATER_THAN, 10));
			sum_t.SelectColumn(3).DecorateCells(size(WIDTH, GREATER_THAN, 11));
			sum_t.SelectColumn(4).DecorateCells(size(WIDTH, GREATER_THAN, 10));

			std::string runDetailsLabel = "Run Details:";

			cpu_panel = vbox({
				text("RESULTS - CPU") | bold | color(Color::Yellow),
				separator(),
				text("Hashes Computed: " + std::to_string(cpuMetrics.batchSize)),
				text("Original Hash:   " + cpuMetrics.hashResult) | color(Color::Yellow),
				text(runDetailsLabel) | bold,
				hbox({ t.Render(), drawScrollbar(totalRuns, startIndex, visibleCount) }),
				separator(),
				text("Summary:") | bold,
				sum_t.Render(),
			});
		} else {
			cpu_panel = vbox({
				text("RESULTS - CPU") | bold | color(Color::Yellow),
				separator(),
				text("No CPU metrics recorded yet.") | dim,
			});
		}

		auto results_box = window(text("3. Benchmark Results") | bold | color(Color::Cyan),
			hbox({
				gpu_panel | flex,
				separator(),
				cpu_panel | flex,
			})
		);

		return vbox({
			text("SHA256 CUDA & CPU BENCHMARK") | bold | color(Color::Cyan) | hcenter,
			separator(),
			parameters_box,
			execution_box,
			results_box,
		});
	});

	auto main_handler = CatchEvent(renderer, [&](Event event) {
		if (event == Event::CtrlC) {
			return true;
		}

		if (event.is_mouse()) {
			auto mouse = event.mouse();
			lastMouseX = mouse.x;
			
			int terminalWidth = screen.dimx();
			int middle = terminalWidth / 2;
			if (mouse.button == Mouse::WheelUp) {
				if (mouse.x < middle) {
					gpuScrollOffset = std::max(0, gpuScrollOffset - 1);
				} else {
					cpuScrollOffset = std::max(0, cpuScrollOffset - 1);
				}
				if (redrawTrigger) redrawTrigger();
				return true;
			}
			if (mouse.button == Mouse::WheelDown) {
				if (mouse.x < middle) {
					int totalRuns = 0;
					{
						std::lock_guard<std::mutex> lock(metricsMutex);
						totalRuns = (int)gpuMetrics.runs.size();
					}
					int maxOffset = std::max(0, totalRuns - kVisibleRunRows);
					gpuScrollOffset = std::min(maxOffset, gpuScrollOffset + 1);
				} else {
					int totalRuns = 0;
					{
						std::lock_guard<std::mutex> lock(metricsMutex);
						totalRuns = (int)cpuMetrics.runs.size();
					}
					int maxOffset = std::max(0, totalRuns - kVisibleRunRows);
					cpuScrollOffset = std::min(maxOffset, cpuScrollOffset + 1);
				}
				if (redrawTrigger) redrawTrigger();
				return true;
			}
		} else {
			int terminalWidth = screen.dimx();
			int middle = terminalWidth / 2;
			if (event == Event::ArrowUp) {
				if (lastMouseX < middle) {
					gpuScrollOffset = std::max(0, gpuScrollOffset - 1);
				} else {
					cpuScrollOffset = std::max(0, cpuScrollOffset - 1);
				}
				if (redrawTrigger) redrawTrigger();
				return true;
			}
			if (event == Event::ArrowDown) {
				if (lastMouseX < middle) {
					int totalRuns = 0;
					{
						std::lock_guard<std::mutex> lock(metricsMutex);
						totalRuns = (int)gpuMetrics.runs.size();
					}
					int maxOffset = std::max(0, totalRuns - kVisibleRunRows);
					gpuScrollOffset = std::min(maxOffset, gpuScrollOffset + 1);
				} else {
					int totalRuns = 0;
					{
						std::lock_guard<std::mutex> lock(metricsMutex);
						totalRuns = (int)cpuMetrics.runs.size();
					}
					int maxOffset = std::max(0, totalRuns - kVisibleRunRows);
					cpuScrollOffset = std::min(maxOffset, cpuScrollOffset + 1);
				}
				if (redrawTrigger) redrawTrigger();
				return true;
			}
		}
		return false;
	});

	screen.Loop(main_handler);
}
