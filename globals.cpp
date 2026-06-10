#include "globals.hpp"

namespace Globals {
	nlohmann::json config;
	nlohmann::json authentication;
	wil::com_ptr<ICoreWebView2> webview;
	wil::com_ptr<ICoreWebView2Controller> controller;
	HWND hwnd = nullptr;
	
	DWORD nodePid = 0;

	HANDLE pipe = nullptr;
	std::wstring pipeName;
}
