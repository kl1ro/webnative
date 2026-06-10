#include "config.hpp"
#include "globals.hpp"
#include <stdexcept>
#include <filesystem>

nlohmann::json& getConfig(const std::string& name) {
	if (!Globals::config.is_null()) return Globals::config;

	if (name.empty()) {
		throw std::runtime_error("Config file name cannot be empty");
	}

	std::wstring appDir = getAppDir();
	std::wstring configPathW = appDir + L"\\" + std::wstring(name.begin(), name.end());
	std::string configPath(configPathW.begin(), configPathW.end());

	// Verify config file exists before opening
	if (!std::filesystem::exists(configPathW)) {
		throw std::runtime_error("Config file does not exist: " + configPath);
	}

	std::ifstream file(configPathW);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open config file: " + configPath);
	}

	try {
		file >> Globals::config;
	}
	catch (const nlohmann::json::exception& e) {
		throw std::runtime_error("Invalid JSON in config file: " + std::string(e.what()));
	}

	if (!file.good() && !file.eof()) {
		throw std::runtime_error("Error reading config file: " + configPath);
	}

	Globals::config["appDir"] = toUtf8(appDir);
	Globals::config["nodePath"] = toUtf8(findNode(appDir));

	return Globals::config;
}

std::wstring getAppDir() {
	wchar_t buf[MAX_PATH] = {};
	DWORD length = GetModuleFileNameW(NULL, buf, MAX_PATH);
	
	if (length == 0) {
		throw std::runtime_error("Failed to get module file name. Error: " + std::to_string(GetLastError()));
	}
	
	if (length == MAX_PATH) {
		throw std::runtime_error("Module file path exceeds MAX_PATH");
	}
	
	std::wstring path(buf, length);
	size_t lastSlash = path.find_last_of(L"\\");
	
	if (lastSlash == std::wstring::npos) {
		throw std::runtime_error("Invalid module path: no directory separator found");
	}
	
	return path.substr(0, lastSlash);
}

std::wstring findNode(const std::wstring& appDir) {
	if (appDir.empty()) {
		return L"node"; // Fallback to PATH
	}
	
	std::wstring localNode = appDir + L"\\node.exe";
	
	try {
		if (std::filesystem::exists(localNode)) {
			return localNode;
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		// Silently fall back to PATH-based node
	}
	
	return L"node"; // Fall back to system PATH
}
