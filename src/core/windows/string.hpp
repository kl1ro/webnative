#pragma once

#include <string>
#include <windows.h>

std::string toUtf8(const std::wstring& wstr);
std::wstring toWString(const std::string& str);
