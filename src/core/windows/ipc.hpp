#pragma once

#include <nlohmann/json.hpp>
#include <wil/com.h>
#include <WebView2.h>

void setupPipe();
void sendToWebview(ICoreWebView2* webview, const nlohmann::json& object);
nlohmann::json getFromNode();
inline std::wstring getPipeName();
