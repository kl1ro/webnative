#include "string.hpp"
#include <stdexcept>

std::string toUtf8(const std::wstring& wstr) {
	if (wstr.empty()) {
		return std::string();
	}
	
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) {
		throw std::runtime_error("Failed to determine UTF-8 buffer size. Error: " + std::to_string(GetLastError()));
	}
	
	std::string result(size - 1, 0);
	int converted = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
	if (converted <= 0) {
		throw std::runtime_error("Failed to convert wide string to UTF-8. Error: " + std::to_string(GetLastError()));
	}
	
	return result;
}

std::wstring toWString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}
	
	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (size <= 0) {
		throw std::runtime_error("Failed to determine wide string buffer size. Error: " + std::to_string(GetLastError()));
	}
	
	std::wstring result(size - 1, 0);
	int converted = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
	if (converted <= 0) {
		throw std::runtime_error("Failed to convert UTF-8 to wide string. Error: " + std::to_string(GetLastError()));
	}
	
	return result;
}
