#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <windows.h>
#include "string.hpp"

nlohmann::json& getConfig(const std::string& name = "webnative.json");
std::wstring getAppDir();
std::wstring findNode(const std::wstring& appDir);
