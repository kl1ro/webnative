#include "config.hpp"
#include "globals.hpp"

nlohmann::json& getConfig(const std::string& name) {
	if (!Globals::config.is_null()) return Globals::config;

	std::wstring appDir = getAppDir();
	std::wstring configPathW = appDir + L"\\" + std::wstring(name.begin(), name.end());
	std::string configPath(configPathW.begin(), configPathW.end());

	std::ifstream file(configPathW);
	if (!file.is_open()) throw std::runtime_error("Could not open config file: " + configPath);

	file >> Globals::config;
	Globals::config["appDir"] = toUtf8(appDir);
	Globals::config["nodePath"] = toUtf8(findNode(appDir));

	return Globals::config;
}

std::wstring getAppDir() {
	wchar_t buf[MAX_PATH];
	GetModuleFileNameW(NULL, buf, MAX_PATH);
	std::wstring path(buf);
	return path.substr(0, path.find_last_of(L"\\"));
}

std::wstring findNode(const std::wstring& appDir) {
	std::wstring localNode = appDir + L"\\node.exe";
	if (GetFileAttributesW(localNode.c_str()) != INVALID_FILE_ATTRIBUTES) return localNode;
	return L"node";
}
