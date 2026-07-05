#pragma once

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <nlohmann/json.hpp>
#include <string>
#include "api.hpp"

void runApplication(HINSTANCE hInstance, int nCmdShow);
HWND createWindow(HINSTANCE hInstance, const nlohmann::json& config);
void createWebview(HWND hwnd, const nlohmann::json& config);
void applyConfigToWindow(HWND hwnd, const nlohmann::json& config);
void loadHtmlToWebview(const std::wstring& appDir);
void applyConfigToWebviewSettings(const nlohmann::json& config);
void onApiCall(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);
void terminateWebviewApp();
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
