/*
 * input_handler.cpp - Input Reading and Validation Implementation
 * Single Responsibility: Read text/file input and validate it
 *
 * Part of SHA256 CUDA Hasher Phase 1
 */

#include "input_handler.h"
#include <fstream>
#include <sys/stat.h>

InputHandler::InputHandler()
{
}

InputHandler::~InputHandler()
{
}

std::vector<BYTE> InputHandler::readFromText(const std::string& text)
{
	clearError();

	if (text.empty()) {
		lastError = "Input text is empty.";
		return {};
	}

	std::vector<BYTE> data(text.begin(), text.end());
	return data;
}

std::vector<BYTE> InputHandler::readFromFile(const std::string& filePath)
{
	clearError();

	if (filePath.empty()) {
		lastError = "File path is empty.";
		return {};
	}

	if (!isValidFilePath(filePath)) {
		lastError = "File does not exist or cannot be accessed: " + filePath;
		return {};
	}

	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		lastError = "Failed to open file: " + filePath;
		return {};
	}

	std::streamsize size = file.tellg();
	if (size <= 0) {
		lastError = "File is empty: " + filePath;
		file.close();
		return {};
	}

	file.seekg(0, std::ios::beg);
	std::vector<BYTE> data(static_cast<size_t>(size));

	if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
		lastError = "Failed to read file contents: " + filePath;
		file.close();
		return {};
	}

	file.close();
	return data;
}

bool InputHandler::isValidFilePath(const std::string& path) const
{
	if (path.empty()) return false;

	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

bool InputHandler::isInputEmpty(const std::string& input) const
{
	return input.empty();
}

bool InputHandler::hasError() const
{
	return !lastError.empty();
}

std::string InputHandler::getErrorMessage() const
{
	return lastError;
}

void InputHandler::clearError()
{
	lastError.clear();
}
