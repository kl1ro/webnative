#pragma once

#include <webkitgtk-6.0/webkit/webkit.h>
#include <nlohmann/json.hpp>
#include <string>
#include "api.hpp"

void runApplication(int argc, char **argv);
void activate(GtkApplication *app);
WebKitWebView* createWebviewWindow(GtkApplication *app, const nlohmann::json& config);
WebKitWebView* createWebview();
void applyConfigToWindow(GtkWidget* window, const nlohmann::json& config);
void loadHtmlToWebview(WebKitWebView* webview, const std::string& html);
void applyConfigToWebkitSettings(WebKitWebView* webview, const nlohmann::json& config);
void onApiCall(WebKitUserContentManager *manager, JSCValue *value);
void terminateWebkitApp();
