#include "webview2.hpp"
#include "globals.hpp"
#include <stdexcept>
#include <filesystem>

// Configuration constants
const wchar_t* WINDOW_CLASS_NAME = L"WebnativeWindow";
const int DEFAULT_WINDOW_WIDTH = 1200;
const int DEFAULT_WINDOW_HEIGHT = 700;
const int DEFAULT_MIN_WIDTH = 500;
const int DEFAULT_MIN_HEIGHT = 700;
const wchar_t* WEBVIEW_DATA_DIR = L"\\webview2_data";
const wchar_t* PUBLIC_HTML_PATH = L"\\public\\index.html";

void runApplication(HINSTANCE hInstance, int nCmdShow) {
    auto& config = getConfig();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClassW(&wc)) {
        throw std::runtime_error("Failed to register window class");
    }

    Globals::hwnd = createWindow(hInstance, config);
    if (!Globals::hwnd) {
        throw std::runtime_error("Failed to create application window");
    }
    ShowWindow(Globals::hwnd, nCmdShow);
    createWebview(Globals::hwnd, config);

    MSG msg = {};

    while (GetMessage(&msg, NULL, 0, 0)) {  
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

HWND createWindow(HINSTANCE hInstance, const nlohmann::json& config) {
    auto windowConfig = config.value("window", nlohmann::json::object());
    int width = windowConfig.value("width", DEFAULT_WINDOW_WIDTH);
    int height = windowConfig.value("height", DEFAULT_WINDOW_HEIGHT);
    std::string titleStr = config.value("name", "Webnative application");
    std::wstring title(titleStr.begin(), titleStr.end());

    HWND hwnd = CreateWindowExW(
        0, WINDOW_CLASS_NAME, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return nullptr;
    }

    applyConfigToWindow(hwnd, config);
    return hwnd;
}

void applyConfigToWindow(HWND hwnd, const nlohmann::json& config) {
    if (!hwnd) return;
    
    auto windowConfig = config.value("window", nlohmann::json::object());
    if (windowConfig.value("fullscreen", false)) ShowWindow(hwnd, SW_MAXIMIZE);

    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        return; // Skip sizing if we can't get window rect
    }
    
    int minWidth = windowConfig.value("minWidth", DEFAULT_MIN_WIDTH);
    int minHeight = windowConfig.value("minHeight", DEFAULT_MIN_HEIGHT);
    SetWindowPos(hwnd, NULL, rect.left, rect.top,
        max(rect.right - rect.left, minWidth),
        max(rect.bottom - rect.top, minHeight),
        SWP_NOZORDER
    );
}

void createWebview(HWND hwnd, const nlohmann::json& config) {
    if (!hwnd) {
        throw std::runtime_error("Invalid window handle passed to createWebview");
    }
    
    std::wstring appDir = toWString(config.value("appDir", "."));
    
    // Validate appDir exists
    try {
        if (!std::filesystem::exists(appDir)) {
            throw std::runtime_error("Application directory does not exist: " + config.value("appDir", "."));
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(std::string("Failed to validate appDir: ") + e.what());
    }
    
    std::wstring dataDir = appDir + WEBVIEW_DATA_DIR;

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, dataDir.c_str(), nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, appDir](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    return result;
                }
                
                HRESULT hr = env->CreateCoreWebView2Controller(hwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, appDir](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(result) || !ctrl) {
                                return result;
                            }
                            Globals::controller = ctrl;
                            if (FAILED(Globals::controller->get_CoreWebView2(&Globals::webview))) {
                                return E_FAIL;
                            }

                            RECT bounds;
                            if (!GetClientRect(hwnd, &bounds)) {
                                return E_FAIL;
                            }
                            
                            if (FAILED(Globals::controller->put_Bounds(bounds))) {
                                return E_FAIL;
                            }

                            if (FAILED(Globals::webview->AddScriptToExecuteOnDocumentCreated(
                                L"window.sendSignalToCpp = (msg) => window.chrome.webview.postMessage(msg);",
                                nullptr
                            ))) {
                                return E_FAIL;
                            }

                            if (FAILED(Globals::webview->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        if (sender && args) {
                                            onApiCall(sender, args);
                                        }
                                        return S_OK;
                                    }
                                ).Get(), nullptr
                            ))) {
                                return E_FAIL;
                            }

                            applyConfigToWebviewSettings(getConfig());
                            loadHtmlToWebview(appDir);

                            return S_OK;
                        }
                    ).Get()
                );
                return hr;
            }
        ).Get()
    );
    
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create WebView2 environment");
    }
}

void loadHtmlToWebview(const std::wstring& appDir) {
    if (!Globals::webview) {
        return;
    }
    
    std::wstring publicPath = L"file:///" + appDir + PUBLIC_HTML_PATH;
    std::replace(publicPath.begin(), publicPath.end(), L'\\', L'/');
    
    if (FAILED(Globals::webview->Navigate(publicPath.c_str()))) {
        // Log navigation error but don't throw - webview may still initialize
    }
}

void applyConfigToWebviewSettings(const nlohmann::json& config) {
    if (!Globals::webview) {
        return;
    }
    
    ICoreWebView2Settings* settings = nullptr;
    if (FAILED(Globals::webview->get_Settings(&settings)) || !settings) {
        return;
    }
    
    // Automatically release settings when scope exits
    auto settingsGuard = wil::scope_exit([settings] { settings->Release(); });

    if (config.value("env", "development") != "development") return;
    
    settings->put_AreDevToolsEnabled(TRUE);
    Globals::webview->OpenDevToolsWindow();
}

void onApiCall(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
    if (!sender || !args) {
        return;
    }
    
    wil::unique_cotaskmem_string message;
    if (FAILED(args->TryGetWebMessageAsString(&message)) || !message.get()) {
        return;
    }
    
    std::wstring wdata(message.get());
    std::string data = toUtf8(wdata);
    handleApiCall(sender, data);
}

void terminateWebviewApp() {
    std::exit(0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (Globals::controller) {
                RECT bounds;
                if (GetClientRect(hwnd, &bounds)) {
                    Globals::controller->put_Bounds(bounds);
                }
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}
