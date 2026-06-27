/*
 * cmd_ui.cpp - Console UI Implementation
 * Single Responsibility: Render clickable/scrollable CMD interface and handle user input events
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#include "cmd_ui.h"
#include <iostream>
#include <sstream>
#include <iomanip>

static std::string formatHashRate(double rate)
{
	std::stringstream ss;
	ss << std::fixed << std::setprecision(2);
	if (rate >= 1e9) {
		ss << (rate / 1e9) << " GH/s";
	} else if (rate >= 1e6) {
		ss << (rate / 1e6) << " MH/s";
	} else if (rate >= 1e3) {
		ss << (rate / 1e3) << " KH/s";
	} else {
		ss << rate << " H/s";
	}
	return ss.str();
}

CmdUI::CmdUI()
	: hOut(INVALID_HANDLE_VALUE)
	, hIn(INVALID_HANDLE_VALUE)
	, consoleWidth(170)
	, consoleHeight(42)
	, batchSizeStr("100000")
	, activeTextbox(0)
	, resultsScrollOffset(0)
	, lastComputeMode("GPU")
	, hasCpuMetrics(false)
	, hasGpuMetrics(false)
	, cpuMetrics{}
	, gpuMetrics{}
	, isDialogOpen(false)
{
}

CmdUI::~CmdUI()
{
	// Restore console mode
	if (hIn != INVALID_HANDLE_VALUE) {
		DWORD mode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
		SetConsoleMode(hIn, mode);
	}
}

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

void CmdUI::initialize(int width, int height)
{
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	hIn = GetStdHandle(STD_INPUT_HANDLE);

	// Set console output to UTF-8 for Unicode box-drawing characters
	SetConsoleOutputCP(65001);

	// Enable virtual terminal processing for ANSI escape sequences
	DWORD dwMode = 0;
	if (GetConsoleMode(hOut, &dwMode)) {
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, dwMode);
	}

	// Send ANSI escape sequence to resize window to 42 rows and 170 columns
	DWORD written;
	std::string resizeSeq = "\x1b[8;42;170t";
	WriteConsoleA(hOut, resizeSeq.c_str(), (DWORD)resizeSeq.size(), &written, NULL);

	// Give the terminal a brief moment to process the resize sequence
	Sleep(50);

	// Query actual size of terminal window or default to 170
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	int actualWidth = 170;
	if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
		int winWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		if (winWidth > actualWidth) {
			actualWidth = winWidth;
		}
	}

	consoleWidth = actualWidth;
	consoleHeight = height;

	// Set console buffer and window size
	COORD bufferSize = { (SHORT)consoleWidth, (SHORT)consoleHeight };
	SetConsoleScreenBufferSize(hOut, bufferSize);

	SMALL_RECT windowSize = { 0, 0, (SHORT)(consoleWidth - 1), (SHORT)(consoleHeight - 1) };
	SetConsoleWindowInfo(hOut, TRUE, &windowSize);

	// Enable mouse and window input
	DWORD mode = ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
	SetConsoleMode(hIn, mode);

	// Hide cursor
	CONSOLE_CURSOR_INFO cursorInfo;
	cursorInfo.dwSize = 1;
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(hOut, &cursorInfo);

	// Set console title
	SetConsoleTitleA("SHA256");

	// Set console font to a TrueType font that supports Unicode
	CONSOLE_FONT_INFOEX fontInfo = {};
	fontInfo.cbSize = sizeof(fontInfo);
	fontInfo.nFont = 0;
	fontInfo.dwFontSize.X = 0;
	fontInfo.dwFontSize.Y = 18;
	fontInfo.FontFamily = FF_DONTCARE;
	fontInfo.FontWeight = FW_NORMAL;
	wcscpy_s(fontInfo.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(hOut, FALSE, &fontInfo);

	// Clear screen
	COORD origin = { 0, 0 };
	FillConsoleOutputCharacterA(hOut, ' ', consoleWidth * consoleHeight, origin, &written);
	FillConsoleOutputAttribute(hOut, COLOR_LABEL, consoleWidth * consoleHeight, origin, &written);
}

void CmdUI::updateDimensions()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
		consoleWidth = csbi.dwSize.X >= 110 ? csbi.dwSize.X : 110;
		consoleHeight = csbi.dwSize.Y;
	}
}

// ======================================================
// Drawing Methods
// ======================================================

void CmdUI::drawFrame()
{
	updateDimensions();
	int colWidth = (consoleWidth - 6) / 2;
	int sepCol = COL_START + colWidth + 1;
	int cpuColStart = COL_START + colWidth + 3;

	clickRegions.clear();

	// Clear the entire interior of the console screen first to prevent any layout artifacts
	DWORD written;
	for (int r = 1; r < consoleHeight - 1; r++) {
		COORD pos = { 1, (SHORT)r };
		FillConsoleOutputCharacterA(hOut, ' ', consoleWidth - 2, pos, &written);
		FillConsoleOutputAttribute(hOut, COLOR_LABEL, consoleWidth - 2, pos, &written);
	}

	drawBox(0, 0, consoleWidth, consoleHeight);
	drawTitle();
	drawHorizontalLine(3, 0, consoleWidth - 1);
	drawTextboxes();
	drawGPUInfo(gpuInfo);
	drawBatchInput();
	drawRunButtons();
	drawRunCPUButton();
	drawClearButton();
	drawHorizontalLine(ROW_RESULTS_START - 1, 0, consoleWidth - 1);

	// Results headers
	std::string gpuTitle = " RESULTS – GPU";
	COORD posGPU = { (SHORT)COL_START, (SHORT)ROW_RESULTS_START };
	WriteConsoleOutputCharacterA(hOut, gpuTitle.c_str(), (DWORD)gpuTitle.size(), posGPU, &written);
	FillConsoleOutputAttribute(hOut, COLOR_SECTION_HEADER, (DWORD)gpuTitle.size(), posGPU, &written);

	std::string cpuTitle = " RESULTS – CPU";
	COORD posCPU = { (SHORT)cpuColStart, (SHORT)ROW_RESULTS_START };
	WriteConsoleOutputCharacterA(hOut, cpuTitle.c_str(), (DWORD)cpuTitle.size(), posCPU, &written);
	FillConsoleOutputAttribute(hOut, COLOR_SECTION_HEADER, (DWORD)cpuTitle.size(), posCPU, &written);

	// Draw vertical separator in the results area
	for (int r = ROW_RESULTS_START; r < consoleHeight - 2; r++) {
		COORD posSep = { (SHORT)sepCol, (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, posSep, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posSep, &written);
	}

	// Redraw existing results if present
	if (hasGpuMetrics) {
		drawResultColumn(gpuMetrics, COL_START, true);
	}
	if (hasCpuMetrics) {
		drawResultColumn(cpuMetrics, cpuColStart, false);
	}

	// Clear the entire speedup row width (including padding columns 84 and 86) and draw vertical separator
	clearLine(consoleHeight - 3, 2, consoleWidth - 3);
	COORD posSep = { (SHORT)sepCol, (SHORT)(consoleHeight - 3) };
	WriteConsoleOutputCharacterA(hOut, "|", 1, posSep, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posSep, &written);

	// Status bar at bottom
	drawStatusBar();

	checkBatchWarning();

	if (isDialogOpen) {
		drawDialog();
	};
}

void CmdUI::drawTitle()
{
	std::string title = " SHA256";
	int centerX = (consoleWidth - (int)title.size()) / 2;

	DWORD written;
	COORD pos = { (SHORT)centerX, (SHORT)ROW_TITLE };
	WriteConsoleOutputCharacterA(hOut, title.c_str(), (DWORD)title.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_TITLE, (DWORD)title.size(), pos, &written);
}

void CmdUI::drawTextboxes()
{
	drawSingleTextbox(ROW_TEXT_INPUT, " Text Input: ", textInput, (activeTextbox == 0), REGION_TEXTBOX_TEXT);
}

void CmdUI::drawSingleTextbox(int row, const std::string& label, const std::string& value, bool active, int textboxId)
{
	DWORD written;
	COORD pos = { (SHORT)COL_START, (SHORT)row };

	// Label
	WriteConsoleOutputCharacterA(hOut, label.c_str(), (DWORD)label.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_LABEL, (DWORD)label.size(), pos, &written);

	// Textbox with filled background (no brackets)
	int tbX = COL_START + (int)label.size();
	int fieldWidth = consoleWidth - tbX - 4;
	if (fieldWidth < 20) fieldWidth = 20;
	std::string displayValue = value;
	if ((int)displayValue.size() > fieldWidth - 2) {
		displayValue = displayValue.substr(displayValue.size() - (fieldWidth - 2));
	}
	std::string boxContent = " " + displayValue;
	while ((int)boxContent.size() < fieldWidth) boxContent += " ";

	// Active: bright white on light blue bg; Inactive: gray on dark blue bg
	WORD tbColor = active ? COLOR_INPUT_ACTIVE : COLOR_INPUT_INACTIVE;
	pos = { (SHORT)tbX, (SHORT)row };
	WriteConsoleOutputCharacterA(hOut, boxContent.c_str(), (DWORD)boxContent.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, tbColor, (DWORD)boxContent.size(), pos, &written);

	registerClickRegion(tbX, row, (int)boxContent.size(), 1, textboxId, label);
}

void CmdUI::drawGPUInfo(const GPUDeviceInfo& info)
{
	DWORD written;

	// GPU Name
	std::string gpuLabel = " GPU: ";
	std::string gpuName = info.valid ? info.deviceName : "No CUDA device detected";
	std::string gpuLine = gpuLabel + gpuName;
	// Pad to fill width
	while ((int)gpuLine.size() < consoleWidth - 4) gpuLine += " ";

	COORD pos = { (SHORT)COL_START, (SHORT)ROW_GPU_INFO };
	WriteConsoleOutputCharacterA(hOut, gpuLine.c_str(), (DWORD)gpuLine.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_LABEL, (DWORD)gpuLabel.size(), pos, &written);
	COORD valPos = { (SHORT)(COL_START + (int)gpuLabel.size()), (SHORT)ROW_GPU_INFO };
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_VALUE, (DWORD)gpuName.size(), valPos, &written);
}

void CmdUI::drawBatchInput()
{
	drawSingleTextbox(ROW_BATCH, " Batch Size: ", batchSizeStr, (activeTextbox == 1), REGION_TEXTBOX_BATCH);
}

void CmdUI::drawRunButtons()
{
	std::string btnLabel = "[ > RUN HASH GPU ]";
	std::string cpuLabel = "[ > RUN HASH CPU ]";
	std::string clrLabel = "[ CLEAR ]";
	int totalWidth = (int)btnLabel.size() + 3 + (int)cpuLabel.size() + 5 + (int)clrLabel.size();
	int startX = (consoleWidth - totalWidth) / 2;
	int row = ROW_BUTTON;

	DWORD written;
	COORD pos = { (SHORT)startX, (SHORT)row };
	WriteConsoleOutputCharacterA(hOut, btnLabel.c_str(), (DWORD)btnLabel.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BUTTON, (DWORD)btnLabel.size(), pos, &written);
	registerClickRegion(startX, row, (int)btnLabel.size(), 1, REGION_RUN_BUTTON, "Run Hash GPU");
}

void CmdUI::drawRunCPUButton()
{
	std::string btnLabel = "[ > RUN HASH GPU ]";
	std::string cpuLabel = "[ > RUN HASH CPU ]";
	std::string clrLabel = "[ CLEAR ]";
	int totalWidth = (int)btnLabel.size() + 3 + (int)cpuLabel.size() + 5 + (int)clrLabel.size();
	int startX = (consoleWidth - totalWidth) / 2;
	int cpuX = startX + (int)btnLabel.size() + 3;
	int row = ROW_BUTTON;

	DWORD written;
	COORD pos = { (SHORT)cpuX, (SHORT)row };
	WriteConsoleOutputCharacterA(hOut, cpuLabel.c_str(), (DWORD)cpuLabel.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BUTTON, (DWORD)cpuLabel.size(), pos, &written);
	registerClickRegion(cpuX, row, (int)cpuLabel.size(), 1, REGION_RUN_BUTTON_CPU, "Run Hash CPU");
}

void CmdUI::drawClearButton()
{
	std::string btnLabel = "[ > RUN HASH GPU ]";
	std::string cpuLabel = "[ > RUN HASH CPU ]";
	std::string clrLabel = "[ CLEAR ]";
	int totalWidth = (int)btnLabel.size() + 3 + (int)cpuLabel.size() + 5 + (int)clrLabel.size();
	int startX = (consoleWidth - totalWidth) / 2;
	int clrX = startX + (int)btnLabel.size() + 3 + (int)cpuLabel.size() + 5;
	int row = ROW_BUTTON;

	DWORD written;
	COORD pos = { (SHORT)clrX, (SHORT)row };
	WriteConsoleOutputCharacterA(hOut, clrLabel.c_str(), (DWORD)clrLabel.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BUTTON_CLEAR, (DWORD)clrLabel.size(), pos, &written);
	registerClickRegion(clrX, row, (int)clrLabel.size(), 1, REGION_CLEAR_BUTTON, "Clear");
}

void CmdUI::drawStatusBar()
{
	int row = consoleHeight - 2;
	DWORD written;

	// Fill entire status bar row with background
	std::string statusText = "  ESC: Quit  |  ENTER: Run  |  TAB: Switch Field  |  Ctrl+V: Paste  |  Ctrl+C: Copy  ";
	while ((int)statusText.size() < consoleWidth - 4) statusText += " ";
	if ((int)statusText.size() > consoleWidth - 4) statusText = statusText.substr(0, consoleWidth - 4);

	COORD pos = { (SHORT)COL_START, (SHORT)row };
	WriteConsoleOutputCharacterA(hOut, statusText.c_str(), (DWORD)statusText.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_STATUSBAR, (DWORD)statusText.size(), pos, &written);
}

void CmdUI::drawDialog()
{
	int dlgWidth = 76;
	int dlgHeight = 7;
	int dlgX = (consoleWidth - dlgWidth) / 2;
	int dlgY = (consoleHeight - dlgHeight) / 2;
	DWORD written;

	// Fill dialog background with spaces
	for (int r = dlgY; r < dlgY + dlgHeight; r++) {
		COORD pos = { (SHORT)dlgX, (SHORT)r };
		FillConsoleOutputCharacterA(hOut, ' ', dlgWidth, pos, &written);
		FillConsoleOutputAttribute(hOut, COLOR_INPUT_INACTIVE, dlgWidth, pos, &written);
	}

	// Draw borders
	// Top border
	std::string topLine(dlgWidth, '-');
	topLine[0] = '+';
	topLine[dlgWidth - 1] = '+';
	COORD posTop = { (SHORT)dlgX, (SHORT)dlgY };
	WriteConsoleOutputCharacterA(hOut, topLine.c_str(), (DWORD)topLine.size(), posTop, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, (DWORD)topLine.size(), posTop, &written);

	// Side borders
	for (int r = dlgY + 1; r < dlgY + dlgHeight - 1; r++) {
		COORD posL = { (SHORT)dlgX, (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, posL, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posL, &written);

		COORD posR = { (SHORT)(dlgX + dlgWidth - 1), (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, posR, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posR, &written);
	}

	// Bottom border
	std::string botLine(dlgWidth, '-');
	botLine[0] = '+';
	botLine[dlgWidth - 1] = '+';
	COORD posBot = { (SHORT)dlgX, (SHORT)(dlgY + dlgHeight - 1) };
	WriteConsoleOutputCharacterA(hOut, botLine.c_str(), (DWORD)botLine.size(), posBot, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, (DWORD)botLine.size(), posBot, &written);

	// Title centered on top border
	std::string title = " Hash Details ";
	int titleX = dlgX + (dlgWidth - (int)title.size()) / 2;
	COORD posTitle = { (SHORT)titleX, (SHORT)dlgY };
	WriteConsoleOutputCharacterA(hOut, title.c_str(), (DWORD)title.size(), posTitle, &written);
	FillConsoleOutputAttribute(hOut, COLOR_SECTION_HEADER, (DWORD)title.size(), posTitle, &written);

	// Display full 64-character hash centered on row dlgY + 2
	int hashX = dlgX + (dlgWidth - 64) / 2;
	COORD posHash = { (SHORT)hashX, (SHORT)(dlgY + 2) };
	WriteConsoleOutputCharacterA(hOut, dialogHashValue.c_str(), 64, posHash, &written);
	FillConsoleOutputAttribute(hOut, COLOR_HASH, 64, posHash, &written);

	// Draw copy & close buttons centered on row dlgY + 4
	std::string btnCopy = "[ COPY HASH ]";
	std::string btnClose = "[ CLOSE ]";
	int buttonsWidth = (int)btnCopy.size() + 4 + (int)btnClose.size();
	int btnStartX = dlgX + (dlgWidth - buttonsWidth) / 2;
	int btnY = dlgY + 4;

	COORD posCopy = { (SHORT)btnStartX, (SHORT)btnY };
	WriteConsoleOutputCharacterA(hOut, btnCopy.c_str(), (DWORD)btnCopy.size(), posCopy, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BUTTON, (DWORD)btnCopy.size(), posCopy, &written);
	registerClickRegion(btnStartX, btnY, (int)btnCopy.size(), 1, REGION_DIALOG_COPY, "Copy Hash");

	int closeX = btnStartX + (int)btnCopy.size() + 4;
	COORD posClose = { (SHORT)closeX, (SHORT)btnY };
	WriteConsoleOutputCharacterA(hOut, btnClose.c_str(), (DWORD)btnClose.size(), posClose, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BUTTON_CLEAR, (DWORD)btnClose.size(), posClose, &written);
	registerClickRegion(closeX, btnY, (int)btnClose.size(), 1, REGION_DIALOG_CLOSE, "Close Dialog");
}

void CmdUI::checkBatchWarning()
{
	clearLine(9, COL_START, consoleWidth - 3);
}

void CmdUI::drawResultsPanel(const GPUMetrics& metrics, bool isGPU)
{
	updateDimensions();
	int colWidth = (consoleWidth - 6) / 2;

	if (isGPU) {
		gpuMetrics = metrics;
		hasGpuMetrics = true;
	} else {
		cpuMetrics = metrics;
		hasCpuMetrics = true;
	}

	// 1. Redraw GPU column (left)
	if (hasGpuMetrics) {
		drawResultColumn(gpuMetrics, COL_START, true);
	} else {
		// Clear GPU column if no metrics
		int startRow = ROW_RESULTS_START + 1;
		for (int r = startRow; r < consoleHeight - 2; r++) {
			clearLine(r, COL_START, COL_START + colWidth - 1);
		}
	}

	// 2. Redraw vertical separator
	DWORD written;
	int sepCol = COL_START + colWidth + 1;
	for (int r = ROW_RESULTS_START; r < consoleHeight - 2; r++) {
		COORD posSep = { (SHORT)sepCol, (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, posSep, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posSep, &written);
	}

	// 3. Redraw CPU column (right)
	int cpuColStart = COL_START + colWidth + 3;
	if (hasCpuMetrics) {
		drawResultColumn(cpuMetrics, cpuColStart, false);
	} else {
		// Clear CPU column if no metrics
		int startRow = ROW_RESULTS_START + 1;
		for (int r = startRow; r < consoleHeight - 2; r++) {
			clearLine(r, cpuColStart, cpuColStart + colWidth - 1);
		}
	}

	// Clear the entire speedup row width (including padding columns 84 and 86) and draw vertical separator
	clearLine(consoleHeight - 3, 2, consoleWidth - 3);
	COORD posSep = { (SHORT)sepCol, (SHORT)(consoleHeight - 3) };
	WriteConsoleOutputCharacterA(hOut, "|", 1, posSep, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posSep, &written);

	// Redraw status bar
	drawStatusBar();
}

void CmdUI::drawResultColumn(const GPUMetrics& metrics, int colStart, bool isGPU)
{
	updateDimensions();
	int colWidth = (consoleWidth - 6) / 2;
	int startRow = ROW_RESULTS_START + 1;
	DWORD written;

	// Clear this specific column's area
	for (int r = startRow; r < consoleHeight - 3; r++) {
		clearLine(r, colStart, colStart + colWidth - 1);
	}

	if (!metrics.valid) {
		if (metrics.errorMessage.empty()) {
			return;
		}
		// Show error - wrap across multiple lines if needed
		std::string errMsg = " Error: " + metrics.errorMessage;
		int maxLen = colWidth - 1;
		int row = startRow + 1;
		int maxRow = consoleHeight - 4;
		while (!errMsg.empty() && row <= maxRow) {
			std::string line = errMsg.substr(0, maxLen);
			errMsg = (int)errMsg.size() > maxLen ? errMsg.substr(maxLen) : "";
			COORD pos = { (SHORT)colStart, (SHORT)row };
			WriteConsoleOutputCharacterA(hOut, line.c_str(), (DWORD)line.size(), pos, &written);
			FillConsoleOutputAttribute(hOut, COLOR_ERROR, (DWORD)line.size(), pos, &written);
			row++;
		}
		return;
	}

	COORD pos;

	// Hashes computed summary
	std::string batchLabel = " Hashes Computed: ";
	std::string batchValue = std::to_string(metrics.batchSize);
	pos = { (SHORT)colStart, (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, batchLabel.c_str(), (DWORD)batchLabel.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_LABEL, (DWORD)batchLabel.size(), pos, &written);
	COORD bvPos = { (SHORT)(colStart + (int)batchLabel.size()), (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, batchValue.c_str(), (DWORD)batchValue.size(), bvPos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_VALUE, (DWORD)batchValue.size(), bvPos, &written);

	// Sample hashes - Only show Original hash
	startRow += 2;
	for (const auto& sample : metrics.sampleHashes) {
		if (sample.first == -1) {
			std::string label = " Original:      ";
			std::string hashVal = sample.second;
			
			// Responsive hash truncation to prevent text wrap on narrower terminals
			int hashMaxLen = colWidth - (int)label.size();
			if ((int)hashVal.size() > hashMaxLen) {
				if (hashMaxLen > 15) {
					hashVal = hashVal.substr(0, hashMaxLen - 11) + "..." + hashVal.substr(hashVal.size() - 8);
				} else {
					hashVal = hashVal.substr(0, hashMaxLen);
				}
			}

			pos = { (SHORT)colStart, (SHORT)startRow };
			WriteConsoleOutputCharacterA(hOut, label.c_str(), (DWORD)label.size(), pos, &written);
			FillConsoleOutputAttribute(hOut, COLOR_RESULT_LABEL, (DWORD)label.size(), pos, &written);
			COORD hPos = { (SHORT)(colStart + (int)label.size()), (SHORT)startRow };
			WriteConsoleOutputCharacterA(hOut, hashVal.c_str(), (DWORD)hashVal.size(), hPos, &written);
			FillConsoleOutputAttribute(hOut, COLOR_HASH, (DWORD)hashVal.size(), hPos, &written);
			registerClickRegion(colStart, startRow, colWidth, 1, REGION_SAMPLE_HASH, sample.second);
			startRow++;
			break; // Only show one original hash
		}
	}

	// Parallel Advantage separator
	startRow++;
	std::string advSep = " --- Parallel Advantage ";
	while ((int)advSep.size() < colWidth) advSep += "-";
	pos = { (SHORT)colStart, (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, advSep.c_str(), (DWORD)advSep.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_SECTION_HEADER, (DWORD)advSep.size(), pos, &written);

	startRow++;
	std::stringstream ssSingle;
	ssSingle << std::fixed << std::setprecision(5) << metrics.singleHashTimeMs;
	std::string singleLabel = "   Single hash (batch=1): ";
	while ((int)singleLabel.size() < 26) singleLabel += " ";
	std::string singleValue = ssSingle.str() + " ms";
	pos = { (SHORT)colStart, (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, singleLabel.c_str(), (DWORD)singleLabel.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_LABEL, (DWORD)singleLabel.size(), pos, &written);
	COORD svPos = { (SHORT)(colStart + (int)singleLabel.size()), (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, singleValue.c_str(), (DWORD)singleValue.size(), svPos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_VALUE, (DWORD)singleValue.size(), svPos, &written);

	startRow++;
	std::stringstream ssBatch;
	ssBatch << std::fixed << std::setprecision(5) << metrics.batchHashTimeMs;
	std::string batchLabel2 = "   Batch (n=" + std::to_string(metrics.batchSize) + "): ";
	if (batchLabel2.size() > 29) batchLabel2 = batchLabel2.substr(0, 29);
	while ((int)batchLabel2.size() < 29) batchLabel2 += " ";
	std::string batchValue2 = ssBatch.str() + " ms";
	pos = { (SHORT)colStart, (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, batchLabel2.c_str(), (DWORD)batchLabel2.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_LABEL, (DWORD)batchLabel2.size(), pos, &written);
	COORD bv2Pos = { (SHORT)(colStart + (int)batchLabel2.size()), (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, batchValue2.c_str(), (DWORD)batchValue2.size(), bv2Pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_VALUE, (DWORD)batchValue2.size(), bv2Pos, &written);

	// Ratio line removed

	// Metrics separator
	startRow++;
	std::string separator2 = " --- Metrics ";
	while ((int)separator2.size() < colWidth) separator2 += "-";
	pos = { (SHORT)colStart, (SHORT)startRow };
	WriteConsoleOutputCharacterA(hOut, separator2.c_str(), (DWORD)separator2.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_SECTION_HEADER, (DWORD)separator2.size(), pos, &written);

	// Compact metrics on two lines
	startRow++;
	std::stringstream ssTime;
	ssTime << std::fixed << std::setprecision(5) << (metrics.executionTime * 1000.0);
	double hashRate = (metrics.executionTime > 0.0) ? (metrics.batchSize / metrics.executionTime) : 0.0;
	std::string rateStr = formatHashRate(hashRate);
	std::string metricsLine1 = "   Runtime: " + ssTime.str() + " ms | Rate: " + rateStr;
	pos = { (SHORT)colStart, (SHORT)startRow };
	if ((int)metricsLine1.size() > colWidth) metricsLine1 = metricsLine1.substr(0, colWidth);
	WriteConsoleOutputCharacterA(hOut, metricsLine1.c_str(), (DWORD)metricsLine1.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_RESULT_VALUE, (DWORD)metricsLine1.size(), pos, &written);

	// Power/Energy line removed
}

void CmdUI::drawStatusMessage(const std::string& msg, bool isGPU, bool isError)
{
	updateDimensions();
	int colWidth = (consoleWidth - 6) / 2;
	int colStart = isGPU ? COL_START : (COL_START + colWidth + 3);
	int row = ROW_RESULTS_START + 1;
	clearLine(row, colStart, colStart + colWidth - 1);

	DWORD written;
	std::string text = " " + msg;
	int maxLen = colWidth - 1;
	int maxRow = consoleHeight - 4;
	WORD color = isError ? COLOR_ERROR : COLOR_STATUS;
	while (!text.empty() && row <= maxRow) {
		std::string line = text.substr(0, maxLen);
		text = (int)text.size() > maxLen ? text.substr(maxLen) : "";
		COORD pos = { (SHORT)colStart, (SHORT)row };
		WriteConsoleOutputCharacterA(hOut, line.c_str(), (DWORD)line.size(), pos, &written);
		FillConsoleOutputAttribute(hOut, color, (DWORD)line.size(), pos, &written);
		row++;
	}
}

void CmdUI::clearResults()
{
	updateDimensions();
	int colWidth = (consoleWidth - 6) / 2;
	int cpuColStart = COL_START + colWidth + 3;
	int sepCol = COL_START + colWidth + 1;

	hasCpuMetrics = false;
	hasGpuMetrics = false;
	cpuMetrics = {};
	gpuMetrics = {};

	// Clear result column data up to row consoleHeight - 4 (leaving consoleHeight - 3 for explicit handling)
	for (int r = ROW_RESULTS_START + 1; r < consoleHeight - 3; r++) {
		clearLine(r, COL_START, COL_START + colWidth - 1);
		clearLine(r, cpuColStart, cpuColStart + colWidth - 1);
	}

	// Wipes the entire speedup row and restores vertical separator to prevent remnants in padding columns 84 and 86
	clearLine(consoleHeight - 3, 2, consoleWidth - 3);
	DWORD written;
	COORD posSep = { (SHORT)sepCol, (SHORT)(consoleHeight - 3) };
	WriteConsoleOutputCharacterA(hOut, "|", 1, posSep, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, posSep, &written);
}

void CmdUI::clearAll()
{
	// Clear text input
	textInput.clear();
	drawTextboxes();

	// Clear results area
	clearResults();
}

// ======================================================
// Event Loop
// ======================================================

void CmdUI::runEventLoop(UIEventCallback callback)
{
	INPUT_RECORD inputRecord;
	DWORD eventsRead;
	bool running = true;

	while (running) {
		if (ReadConsoleInputA(hIn, &inputRecord, 1, &eventsRead) && eventsRead > 0) {
			switch (inputRecord.EventType) {
				case MOUSE_EVENT: {
					MOUSE_EVENT_RECORD mouseEvent = inputRecord.Event.MouseEvent;
					if (mouseEvent.dwEventFlags == 0) {
						// Button click
						if (mouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
							handleMouseClick(mouseEvent.dwMousePosition.X,
										   mouseEvent.dwMousePosition.Y, callback);
						}
					}
					else if (mouseEvent.dwEventFlags == MOUSE_WHEELED) {
						// Scroll wheel
						int delta = (short)HIWORD(mouseEvent.dwButtonState);
						handleScrollWheel(delta);
					}
					break;
				}
				case KEY_EVENT: {
					if (inputRecord.Event.KeyEvent.bKeyDown) {
						handleKeyPress(inputRecord.Event.KeyEvent, callback);
					}
					break;
				}
				default:
					break;
			}
		}
	}
}

// ======================================================
// Input Handling
// ======================================================

void CmdUI::handleMouseClick(int x, int y, UIEventCallback& callback)
{
	ClickRegion clickedRegion;
	bool found = false;
	for (const auto& region : clickRegions) {
		if (x >= region.bounds.Left && x <= region.bounds.Right &&
			y >= region.bounds.Top && y <= region.bounds.Bottom) {
			clickedRegion = region;
			found = true;
			break;
		}
	}

	if (isDialogOpen) {
		if (found) {
			if (clickedRegion.id == REGION_DIALOG_COPY) {
				copyToClipboard(dialogHashValue);
			} else if (clickedRegion.id == REGION_DIALOG_CLOSE) {
				isDialogOpen = false;
				drawFrame();
			}
		} else {
			// Clicked outside dialog bounds -> close dialog
			int dlgWidth = 76;
			int dlgHeight = 7;
			int dlgX = (consoleWidth - dlgWidth) / 2;
			int dlgY = (consoleHeight - dlgHeight) / 2;
			if (x < dlgX || x >= dlgX + dlgWidth || y < dlgY || y >= dlgY + dlgHeight) {
				isDialogOpen = false;
				drawFrame();
			}
		}
		return;
	}

	if (!found) return;

	switch (clickedRegion.id) {
		case REGION_RUN_BUTTON:
			lastComputeMode = "GPU";
			if (callback) callback(UIEvent::RUN_CLICKED);
			break;

		case REGION_RUN_BUTTON_CPU:
			lastComputeMode = "CPU";
			if (callback) callback(UIEvent::RUN_CLICKED_CPU);
			break;

		case REGION_CLEAR_BUTTON:
			if (callback) callback(UIEvent::CLEAR_CLICKED);
			break;

		case REGION_TEXTBOX_TEXT:
			activeTextbox = 0;
			drawTextboxes();
			drawBatchInput();
			break;

		case REGION_TEXTBOX_BATCH:
			activeTextbox = 1;
			drawTextboxes();
			drawBatchInput();
			break;

		case REGION_SAMPLE_HASH:
			isDialogOpen = true;
			dialogHashValue = clickedRegion.label;
			drawFrame();
			break;

		default:
			break;
	}
}

void CmdUI::handleKeyPress(KEY_EVENT_RECORD keyEvent, UIEventCallback& callback)
{
	char ch = keyEvent.uChar.AsciiChar;
	WORD vk = keyEvent.wVirtualKeyCode;

	if (isDialogOpen) {
		if (vk == VK_ESCAPE || vk == VK_RETURN) {
			isDialogOpen = false;
			drawFrame();
		}
		return;
	}

	// ESC to quit
	if (vk == VK_ESCAPE) {
		if (callback) callback(UIEvent::QUIT);
		return;
	}

	// ENTER triggers run (default to GPU mode for backward compatibility)
	if (vk == VK_RETURN) {
		lastComputeMode = "GPU";
		if (callback) callback(UIEvent::RUN_CLICKED);
		return;
	}

	// TAB cycles between textboxes
	if (vk == VK_TAB) {
		activeTextbox = (activeTextbox == 0) ? 1 : 0;
		drawTextboxes();
		drawBatchInput();
		return;
	}

	// Route character to active textbox
	std::string* targetStr = nullptr;
	int targetRow = 0;
	std::string targetLabel;

	switch (activeTextbox) {
		case 0:
			targetStr = &textInput;
			targetRow = ROW_TEXT_INPUT;
			targetLabel = " Text Input: ";
			break;
		case 1:
			targetStr = &batchSizeStr;
			targetRow = ROW_BATCH;
			targetLabel = " Batch Size: ";
			break;
		default:
			return;
	}

	// Backspace
	if (vk == VK_BACK) {
		if (!targetStr->empty()) {
			targetStr->pop_back();
		}
	}
	// Ctrl+C: copy active textbox content
	else if (vk == 'C' && (keyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
		copyToClipboard(*targetStr);
		return;
	}
	// Ctrl+V: paste into active textbox
	else if (vk == 'V' && (keyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
		std::string clipText = pasteFromClipboard();
		if (activeTextbox == 1) {
			// Batch size: filter to digits only
			for (char c : clipText) {
				if (c >= '0' && c <= '9') {
					targetStr->push_back(c);
				}
			}
		} else {
			targetStr->append(clipText);
		}
	}
	// Ctrl+A: select all (no-op for now)
	else if (vk == 'A' && (keyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
		return;
	}
	// Printable character
	else if (ch >= 32 && ch <= 126) {
		// For batch size, only accept digits
		if (activeTextbox == 1) {
			if (ch >= '0' && ch <= '9') {
				targetStr->push_back(ch);
			}
		} else {
			targetStr->push_back(ch);
		}
	}
	else {
		return; // Non-printable, ignore
	}

	// Redraw the affected textbox
	bool active = true;
	drawSingleTextbox(targetRow, targetLabel, *targetStr, active,
		(activeTextbox == 0) ? REGION_TEXTBOX_TEXT : REGION_TEXTBOX_BATCH);

	if (activeTextbox == 1) {
		checkBatchWarning();
	}
}

void CmdUI::handleScrollWheel(int delta)
{
	// Scroll results panel (future: implement virtual buffer scrolling)
	if (delta > 0) {
		resultsScrollOffset = (resultsScrollOffset > 0) ? resultsScrollOffset - 1 : 0;
	} else {
		resultsScrollOffset++;
	}
}

// ======================================================
// Clipboard Methods
// ======================================================

void CmdUI::copyToClipboard(const std::string& text)
{
	if (text.empty()) return;
	if (!OpenClipboard(nullptr)) return;

	EmptyClipboard();

	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
	if (hMem) {
		char* pMem = static_cast<char*>(GlobalLock(hMem));
		if (pMem) {
			memcpy(pMem, text.c_str(), text.size() + 1);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}
	}

	CloseClipboard();
}

std::string CmdUI::pasteFromClipboard()
{
	std::string result;
	if (!OpenClipboard(nullptr)) return result;

	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData) {
		const char* pData = static_cast<const char*>(GlobalLock(hData));
		if (pData) {
			// Copy only printable characters (skip newlines, tabs, etc.)
			for (const char* p = pData; *p != '\0'; ++p) {
				if (*p >= 32 && *p <= 126) {
					result.push_back(*p);
				}
			}
			GlobalUnlock(hData);
		}
	}

	CloseClipboard();
	return result;
}

// ======================================================
// Utility Methods
// ======================================================

void CmdUI::setColor(WORD color)
{
	SetConsoleTextAttribute(hOut, color);
}

void CmdUI::resetColor()
{
	SetConsoleTextAttribute(hOut, COLOR_LABEL);
}

void CmdUI::setCursor(int x, int y)
{
	COORD pos = { (SHORT)x, (SHORT)y };
	SetConsoleCursorPosition(hOut, pos);
}

void CmdUI::clearLine(int y, int startX, int endX)
{
	DWORD written;
	int len = endX - startX + 1;
	COORD pos = { (SHORT)startX, (SHORT)y };
	FillConsoleOutputCharacterA(hOut, ' ', len, pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_LABEL, len, pos, &written);
}

void CmdUI::drawBox(int x, int y, int width, int height)
{
	DWORD written;

	// Top border: +====...====+
	std::string topLine(width, '=');
	topLine[0] = '+';
	topLine[width - 1] = '+';
	COORD pos = { (SHORT)x, (SHORT)y };
	WriteConsoleOutputCharacterA(hOut, topLine.c_str(), (DWORD)topLine.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, (DWORD)topLine.size(), pos, &written);

	// Side borders
	for (int r = y + 1; r < y + height - 1; r++) {
		pos = { (SHORT)x, (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, pos, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, pos, &written);
		pos = { (SHORT)(x + width - 1), (SHORT)r };
		WriteConsoleOutputCharacterA(hOut, "|", 1, pos, &written);
		FillConsoleOutputAttribute(hOut, COLOR_BORDER, 1, pos, &written);
	}

	// Bottom border: +====...====+
	std::string botLine(width, '=');
	botLine[0] = '+';
	botLine[width - 1] = '+';
	pos = { (SHORT)x, (SHORT)(y + height - 1) };
	WriteConsoleOutputCharacterA(hOut, botLine.c_str(), (DWORD)botLine.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, (DWORD)botLine.size(), pos, &written);
}

void CmdUI::drawHorizontalLine(int y, int startX, int endX)
{
	DWORD written;
	int len = endX - startX + 1;
	std::string line(len, '=');
	line[0] = '+';
	line[len - 1] = '+';
	COORD pos = { (SHORT)startX, (SHORT)y };
	WriteConsoleOutputCharacterA(hOut, line.c_str(), (DWORD)line.size(), pos, &written);
	FillConsoleOutputAttribute(hOut, COLOR_BORDER, (DWORD)line.size(), pos, &written);
}

void CmdUI::registerClickRegion(int x, int y, int width, int height, int id, const std::string& label)
{
	ClickRegion region;
	region.bounds.Left = (SHORT)x;
	region.bounds.Top = (SHORT)y;
	region.bounds.Right = (SHORT)(x + width - 1);
	region.bounds.Bottom = (SHORT)(y + height - 1);
	region.id = id;
	region.label = label;
	clickRegions.push_back(region);
}

int CmdUI::hitTestRegion(int x, int y)
{
	for (const auto& region : clickRegions) {
		if (x >= region.bounds.Left && x <= region.bounds.Right &&
			y >= region.bounds.Top && y <= region.bounds.Bottom) {
			return region.id;
		}
	}
	return -1;
}

// ======================================================
// Getters
// ======================================================

std::string CmdUI::getTextInput() const
{
	return textInput;
}

int CmdUI::getBatchSize() const
{
	if (batchSizeStr.empty()) return 1;
	try {
		int val = std::stoi(batchSizeStr);
		return (val > 0) ? val : 1;
	}
	catch (...) {
		return 1;
	}
}

void CmdUI::setGPUInfo(const GPUDeviceInfo& info)
{
	gpuInfo = info;
}
