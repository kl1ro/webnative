#pragma once

#include "ipc.hpp"
#include <WebView2.h>

void handleApiCall(ICoreWebView2* webview, const std::string& data);
void authenticate();
