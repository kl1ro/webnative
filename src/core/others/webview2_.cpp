#pragma once

#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <wrl.h>
#include <algorithm>
#include <string>
#include <vector>
#include <wil/com.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "nlohmann/json.hpp"
#include "globals.hpp"
#include "utils.hpp"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
//  Forward declarations
// ============================================================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void onApiCall(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);
void handleApiCall(ICoreWebView2* sender, const std::string& data);
HWND createWindow(HINSTANCE hInstance, const nlohmann::json& config);
void applyConfigToWindow(HWND hwnd, const nlohmann::json& config);
void createWebview(HWND hwnd, const nlohmann::json& config);
void loadHtmlToWebview(const std::wstring& appDir);
void applyConfigToWebviewSettings(const nlohmann::json& config);
void terminateWebviewApp();

// ============================================================================
//  Helper functions
// ============================================================================

// Read an IStream into a byte vector
static std::vector<BYTE> readIStream(IStream* stream) {
    std::vector<BYTE> buffer;
    if (!stream) return buffer;
    BYTE temp[8192];
    ULONG bytesRead = 0;
    while (SUCCEEDED(stream->Read(temp, sizeof(temp), &bytesRead)) && bytesRead > 0) {
        buffer.insert(buffer.end(), temp, temp + bytesRead);
    }
    return buffer;
}

// Create an IStream from a byte vector
static Microsoft::WRL::ComPtr<IStream> createStreamFromBuffer(const std::vector<BYTE>& buffer) {
    Microsoft::WRL::ComPtr<IStream> stream;
    if (buffer.empty()) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 1);
        if (hMem) {
            GlobalLock(hMem);
            GlobalUnlock(hMem);
            CreateStreamOnHGlobal(hMem, TRUE, &stream);
        }
        return stream;
    }
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, buffer.size());
    if (!hMem) return stream;
    void* pMem = GlobalLock(hMem);
    if (pMem) {
        memcpy(pMem, buffer.data(), buffer.size());
        GlobalUnlock(hMem);
        CreateStreamOnHGlobal(hMem, TRUE, &stream);
    } else {
        GlobalFree(hMem);
    }
    return stream;
}

// Convert wstring to lowercase
static std::wstring toLowerW(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

// Check if URL scheme is HTTP or HTTPS
static bool isHttpUrl(const std::wstring& url) {
    std::wstring lower = toLowerW(url);
    return lower.find(L"http://") == 0 || lower.find(L"https://") == 0;
}

// Parse URL into components using WinHttpCrackUrl
static bool parseUrl(const std::wstring& url,
                     std::wstring& scheme,
                     std::wstring& host,
                     INTERNET_PORT& port,
                     std::wstring& path,
                     bool& isHttps)
{
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp))
        return false;

    isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    port    = urlComp.nPort;

    if (urlComp.lpszScheme)
        scheme.assign(urlComp.lpszScheme, urlComp.dwSchemeLength);
    if (urlComp.lpszHostName)
        host.assign(urlComp.lpszHostName, urlComp.dwHostNameLength);

    path.clear();
    if (urlComp.lpszUrlPath)
        path.append(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.lpszExtraInfo)
        path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);

    if (path.empty())
        path = L"/";

    return true;
}

// Extract host from a URL string
static std::wstring extractHostFromUrl(const std::wstring& url) {
    std::wstring host;
    INTERNET_PORT port;
    std::wstring scheme, path;
    bool isHttps;
    if (parseUrl(url, scheme, host, port, path, isHttps))
        return host;
    return L"";
}

// Get all request headers as a string (excluding Host and Content-Length)
static std::wstring getRequestHeadersString(ICoreWebView2WebResourceRequest* request) {
    std::wstring headersStr;
    Microsoft::WRL::ComPtr<ICoreWebView2HttpRequestHeaders> reqHeaders;
    if (FAILED(request->get_Headers(&reqHeaders)) || !reqHeaders)
        return headersStr;

    Microsoft::WRL::ComPtr<ICoreWebView2HttpHeadersCollectionIterator> iterator;
    if (FAILED(reqHeaders->GetIterator(&iterator)) || !iterator)
        return headersStr;

    BOOL hasCurrent = FALSE;
    iterator->get_HasCurrentHeader(&hasCurrent);
    while (hasCurrent) {
        wil::unique_cotaskmem_string name;
        wil::unique_cotaskmem_string value;
        iterator->GetCurrentHeader(&name, &value);
        std::wstring nameStr(name.get());
        // Skip Host (WinHttpConnect sets it) and Content-Length (WinHttpSendRequest sets it)
        if (_wcsicmp(nameStr.c_str(), L"Host") != 0 &&
            _wcsicmp(nameStr.c_str(), L"Content-Length") != 0)
        {
            headersStr += nameStr + L": " + std::wstring(value.get()) + L"\r\n";
        }
        iterator->MoveNext(&hasCurrent);
    }
    return headersStr;
}

// Check if a request header exists and get its value
static bool getRequestHeaderValue(ICoreWebView2WebResourceRequest* request,
                                  const std::wstring& headerName,
                                  std::wstring& outValue)
{
    Microsoft::WRL::ComPtr<ICoreWebView2HttpRequestHeaders> reqHeaders;
    if (FAILED(request->get_Headers(&reqHeaders)) || !reqHeaders)
        return false;

    wil::unique_cotaskmem_string value;
    if (SUCCEEDED(reqHeaders->GetHeader(headerName.c_str(), &value)) && value) {
        outValue = value.get();
        return true;
    }
    return false;
}

// ============================================================================
//  Response Header Modification (CORS + CSP + PNA)
// ============================================================================

// Modify response headers: remove CSP headers and/or add CORS headers
// Also handles PNA (Private Network Access) headers
static std::wstring modifyResponseHeaders(const std::wstring& originalHeaders,
                                          bool removeCsp,
                                          bool addCors,
                                          bool addPna,
                                          const nlohmann::json& config,
                                          const std::vector<BYTE>& body)
{
    auto security = config.value("security", nlohmann::json::object());

    // Parse original headers into a vector of pairs
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    size_t pos = 0;
    while (pos < originalHeaders.size()) {
        size_t lineEnd = originalHeaders.find(L"\r\n", pos);
        if (lineEnd == std::wstring::npos) lineEnd = originalHeaders.size();
        std::wstring line = originalHeaders.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;

        if (line.empty()) continue;

        size_t colon = line.find(L':');
        if (colon != std::wstring::npos) {
            std::wstring name = line.substr(0, colon);
            std::wstring value = line.substr(colon + 1);
            // Trim whitespace from value
            size_t start = value.find_first_not_of(L" \t");
            if (start != std::wstring::npos) {
                size_t end = value.find_last_not_of(L" \t\r\n");
                value = value.substr(start, end - start + 1);
            } else {
                value.clear();
            }
            // Trim whitespace from name
            size_t ns = name.find_first_not_of(L" \t");
            if (ns != std::wstring::npos) {
                size_t ne = name.find_last_not_of(L" \t\r\n");
                name = name.substr(ns, ne - ns + 1);
            }
            headers.emplace_back(name, value);
        }
    }

    // Remove CSP-related headers
    if (removeCsp) {
        headers.erase(
            std::remove_if(headers.begin(), headers.end(),
                [](const std::pair<std::wstring, std::wstring>& h) {
                    std::wstring name = toLowerW(h.first);
                    return name == L"content-security-policy" ||
                           name == L"content-security-policy-report-only" ||
                           name == L"x-content-security-policy" ||
                           name == L"x-webkit-csp";
                }),
            headers.end()
        );
    }

    // Remove existing CORS headers before adding new ones (avoid duplicates)
    if (addCors) {
        headers.erase(
            std::remove_if(headers.begin(), headers.end(),
                [](const std::pair<std::wstring, std::wstring>& h) {
                    std::wstring name = toLowerW(h.first);
                    return name.find(L"access-control-") == 0;
                }),
            headers.end()
        );

        // Add CORS headers from config
        headers.emplace_back(L"Access-Control-Allow-Origin",
            toWString(security.value("corsAllowOrigin", "*")));
        headers.emplace_back(L"Access-Control-Allow-Methods",
            toWString(security.value("corsAllowMethods",
                "GET, POST, PUT, DELETE, PATCH, OPTIONS")));
        headers.emplace_back(L"Access-Control-Allow-Headers",
            toWString(security.value("corsAllowHeaders", "*")));
        if (security.value("corsAllowCredentials", false)) {
            headers.emplace_back(L"Access-Control-Allow-Credentials", L"true");
        }
        headers.emplace_back(L"Access-Control-Max-Age",
            toWString(security.value("corsMaxAge", "86400")));
    }

    // Add PNA (Private Network Access) response header
    // https://wicg.github.io/private-network-access/
    if (addPna) {
        // Remove existing PNA header if present
        headers.erase(
            std::remove_if(headers.begin(), headers.end(),
                [](const std::pair<std::wstring, std::wstring>& h) {
                    std::wstring name = toLowerW(h.first);
                    return name == L"access-control-allow-private-network";
                }),
            headers.end()
        );
        headers.emplace_back(L"Access-Control-Allow-Private-Network", L"true");
    }

    // Handle Transfer-Encoding: chunked — replace with Content-Length
    bool hasChunked = false;
    for (auto& h : headers) {
        if (toLowerW(h.first) == L"transfer-encoding" &&
            toLowerW(h.second).find(L"chunked") != std::wstring::npos) {
            hasChunked = true;
            h.second = L""; // Mark for removal
        }
    }
    if (hasChunked) {
        headers.erase(
            std::remove_if(headers.begin(), headers.end(),
                [](const std::pair<std::wstring, std::wstring>& h) {
                    return h.second.empty() && toLowerW(h.first) == L"transfer-encoding";
                }),
            headers.end()
        );
        // Add Content-Length based on actual body size
        headers.emplace_back(L"Content-Length", std::to_wstring(body.size()));
    }

    // Rebuild headers string
    std::wstring result;
    for (const auto& h : headers) {
        result += h.first + L": " + h.second + L"\r\n";
    }
    return result;
}

// ============================================================================
//  Security Configuration — Browser Arguments
// ============================================================================

// Build Chromium browser arguments string from security + features config
static std::wstring buildBrowserArguments(const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    auto features = config.value("features", nlohmann::json::object());
    std::vector<std::wstring> args;

    // --- Security flags ---

    // Disables same-origin policy and CORS enforcement in Chromium.
    if (security.value("disableWebSecurity", false))
        args.push_back(L"--disable-web-security");

    // Allows mixed-content (HTTP resources on HTTPS pages).
    if (security.value("allowInsecureContent", false))
        args.push_back(L"--allow-running-insecure-content");

    // Allows file:// URLs to access other file:// URLs via XHR/fetch.
    if (security.value("allowFileAccessFromFiles", false))
        args.push_back(L"--allow-file-access-from-files");

    // Allow file access (general)
    if (security.value("allowFileAccess", false))
        args.push_back(L"--allow-file-access");

    // Disables site-per-process and origin isolation.
    if (security.value("disableStrictIsolation", false)) {
        args.push_back(L"--disable-site-isolation-trials");
        args.push_back(L"--disable-features=IsolateOrigins,site-per-process");
    }

    // Enable experimental web platform features
    if (security.value("experimentalPlatformApi", false))
        args.push_back(L"--enable-experimental-web-platform-features");

    // Ignores SSL/TLS certificate validation errors.
    if (security.value("ignoreCertificateErrors", false))
        args.push_back(L"--ignore-certificate-errors");

    // Treats specified insecure origins as secure origins.
    auto secureOrigins = security.value("markOriginsAsSecure", nlohmann::json::array());
    if (!secureOrigins.empty()) {
        std::wstring origins;
        for (size_t i = 0; i < secureOrigins.size(); i++) {
            if (i > 0) origins += L",";
            origins += toWString(secureOrigins[i].get<std::string>());
        }
        args.push_back(L"--unsafely-treat-insecure-origin-as-secure=" + origins);
    }

    // Removes the "navigator.webdriver" flag and automation indicators.
    if (security.value("disableAutomationFlag", false)) {
        args.push_back(L"--disable-features=AutomationControlled");
        args.push_back(L"--disable-blink-features=AutomationControlled");
    }

    // Prevents Chromium from throttling timers in background tabs.
    if (security.value("disableBackgroundThrottling", false)) {
        args.push_back(L"--disable-background-timer-throttling");
        args.push_back(L"--disable-renderer-backgrounding");
        args.push_back(L"--disable-backgrounding-occluded-windows");
    }

    // Enables SharedArrayBuffer without requiring cross-origin isolation.
    if (security.value("enableSharedArrayBuffer", false)) {
        args.push_back(L"--enable-features=SharedArrayBuffer");
        args.push_back(L"--enable-features=CrossOriginIsolation");
    }

    // --- Private Network Access (PNA) flags ---
    // https://wicg.github.io/private-network-access/
    // https://developer.chrome.com/blog/private-network-access-update-2024-03
    auto pnaConfig = features.value("pna", nlohmann::json::object());
    if (pnaConfig.value("enabled", false)) {
        if (pnaConfig.value("disableEnforcement", false)) {
            // Completely disable PNA enforcement
            args.push_back(L"--disable-features=BlockInsecurePrivateNetworkRequests");
            args.push_back(L"--disable-features=PrivateNetworkAccessNonSecureContextsEnforcement");
        }
        if (pnaConfig.value("ignoreWorkerErrors", false)) {
            args.push_back(L"--disable-features=PrivateNetworkAccessIgnoreWorkerErrors");
        }
        if (pnaConfig.value("ignoreNavigationErrors", false)) {
            args.push_back(L"--disable-features=PrivateNetworkAccessIgnoreNavigationErrors");
        }
    }

    // Legacy security.allowPrivateNetworkRequests flag
    if (security.value("allowPrivateNetworkRequests", false)) {
        args.push_back(L"--disable-features=BlockInsecurePrivateNetworkRequests");
    }

    // Treats localhost as a secure origin.
    if (security.value("allowInsecureLocalhost", false))
        args.push_back(L"--allow-insecure-localhost");

    if (security.value("disableNotifications", false))
        args.push_back(L"--disable-notifications");

    if (security.value("disableExtensions", false))
        args.push_back(L"--disable-extensions");

    // NOT recommended for production
    if (security.value("noSandbox", false))
        args.push_back(L"--no-sandbox");

    // --- Feature flags ---
    if (features.value("autofill", false))
        args.push_back(L"--enable-autofill");
    if (features.value("experimentalWebPlatformFeatures", false))
        args.push_back(L"--enable-experimental-web-platform-features");
    if (features.value("webGPU", false))
        args.push_back(L"--enable-features=WebGPU");
    if (features.value("webBluetooth", false))
        args.push_back(L"--enable-features=WebBluetooth");
    if (features.value("webUSB", false))
        args.push_back(L"--enable-features=WebUSB");
    if (features.value("webSerial", false))
        args.push_back(L"--enable-features=WebSerial");
    if (features.value("webHID", false))
        args.push_back(L"--enable-features=WebHID");
    if (features.value("webNFC", false))
        args.push_back(L"--enable-features=WebNFC");
    if (features.value("webShare", false))
        args.push_back(L"--enable-features=WebShare");
    if (features.value("webAuthn", false))
        args.push_back(L"--enable-features=WebAuthn");
    if (features.value("clipboardWrite", false))
        args.push_back(L"--enable-features=ClipboardWrite");
    if (features.value("fullscreen", false))
        args.push_back(L"--enable-features=Fullscreen");
    if (features.value("pictureInPicture", false))
        args.push_back(L"--enable-features=PictureInPicture");
    if (features.value("virtualReality", false))
        args.push_back(L"--enable-features=WebVR");
    if (features.value("augmentedReality", false))
        args.push_back(L"--enable-features=WebXR");
    if (features.value("sensors", false))
        args.push_back(L"--enable-features=Sensors");
    if (features.value("wakeLock", false))
        args.push_back(L"--enable-features=WakeLock");
    if (features.value("contacts", false))
        args.push_back(L"--enable-features=Contacts");
    if (features.value("handwriting", false))
        args.push_back(L"--enable-features=HandwritingRecognition");
    if (features.value("computePressure", false))
        args.push_back(L"--enable-features=ComputePressure");
    if (features.value("smartCard", false))
        args.push_back(L"--enable-features=SmartCard");
    if (features.value("webTransport", false))
        args.push_back(L"--enable-features=WebTransport");
    if (features.value("federatedCredentials", false))
        args.push_back(L"--enable-features=FedCm");
    if (features.value("digitalGoods", false))
        args.push_back(L"--enable-features=DigitalGoods");
    if (features.value("paymentRequest", false))
        args.push_back(L"--enable-features=PaymentRequest");

    // --- Custom flags ---
    auto customFlags = security.value("customFlags", nlohmann::json::array());
    for (const auto& flag : customFlags) {
        args.push_back(toWString(flag.get<std::string>()));
    }

    // Join all arguments
    std::wstring result;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) result += L" ";
        result += args[i];
    }
    return result;
}

// Create ICoreWebView2EnvironmentOptions with additional browser arguments
static Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions>
createEnvironmentOptions(const std::wstring& browserArgs)
{
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> options;
    HRESULT hr = CoCreateInstance(
        __uuidof(CoreWebView2EnvironmentOptions),
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&options)
    );
    if (SUCCEEDED(hr) && options && !browserArgs.empty()) {
        options->put_AdditionalBrowserArguments(browserArgs.c_str());
    }
    return options;
}

// ============================================================================
//  Security Configuration — Host Allowlist
// ============================================================================

// Check if a host is in the allowed hosts list
static bool isHostAllowed(const std::wstring& host, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());

    // If allowAllHosts is true, allow everything
    if (security.value("allowAllHosts", false))
        return true;

    // Check allowedHosts list
    auto allowedHosts = security.value("allowedHosts", nlohmann::json::array());
    std::string hostUtf8(host.begin(), host.end());
    for (const auto& h : allowedHosts) {
        std::string allowed = h.get<std::string>();
        // Case-insensitive comparison
        if (_stricmp(hostUtf8.c_str(), allowed.c_str()) == 0)
            return true;
    }

    return false;
}

// Check if a URL should be intercepted by the CORS/CSP handler
static bool shouldInterceptUrl(const std::wstring& url, const nlohmann::json& config) {
    // Only intercept HTTP/HTTPS requests
    if (!isHttpUrl(url))
        return false;

    std::wstring host = extractHostFromUrl(url);
    if (host.empty())
        return false;

    return isHostAllowed(host, config);
}

// ============================================================================
//  Security Configuration — CSP Meta Tag Removal (Script Injection)
// ============================================================================

// Inject a script that removes CSP meta tags from the DOM
static void injectCspMetaRemover(ICoreWebView2* webview) {
    std::wstring script = LR"(
        (function() {
            'use strict';

            var removeCspMeta = function() {
                var selectors = [
                    'meta[http-equiv="Content-Security-Policy"]',
                    'meta[http-equiv="Content-Security-Policy-Report-Only"]',
                    'meta[http-equiv="X-Content-Security-Policy"]',
                    'meta[http-equiv="X-WebKit-CSP"]'
                ];
                selectors.forEach(function(sel) {
                    var elements = document.querySelectorAll(sel);
                    elements.forEach(function(el) { el.remove(); });
                });
            };

            // Remove existing CSP meta tags
            removeCspMeta();

            // Observe DOM changes and remove CSP meta tags as they are added
            if (document.documentElement) {
                var observer = new MutationObserver(function() {
                    removeCspMeta();
                });
                observer.observe(document.documentElement, {
                    childList: true,
                    subtree: true
                });
            }
        })();
    )";
    webview->AddScriptToExecuteOnDocumentCreated(script.c_str(), nullptr);
}

// ============================================================================
//  Security Configuration — CORS Preflight Handler (with PNA support)
// ============================================================================

// Handle CORS preflight (OPTIONS) requests, including PNA preflight.
// PNA preflight includes header: Access-Control-Request-Private-Network: true
// Server must respond with: Access-Control-Allow-Private-Network: true
// https://wicg.github.io/private-network-access/
static void handleCorsPreflight(
    ICoreWebView2WebResourceRequestedEventArgs* args,
    ICoreWebView2Environment* env,
    const nlohmann::json& config)
{
    auto security = config.value("security", nlohmann::json::object());
    auto features = config.value("features", nlohmann::json::object());
    auto pnaConfig = features.value("pna", nlohmann::json::object());

    // Check if this is a PNA preflight request
    bool isPnaPreflight = false;
    if (pnaConfig.value("autoAllowPreflight", false)) {
        Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request;
        if (SUCCEEDED(args->get_Request(&request)) && request) {
            std::wstring pnaHeader;
            if (getRequestHeaderValue(request.Get(),
                    L"Access-Control-Request-Private-Network", pnaHeader)) {
                std::wstring lowerVal = toLowerW(pnaHeader);
                if (lowerVal == L"true") {
                    isPnaPreflight = true;
                }
            }
        }
    }

    // Build CORS response headers
    std::wstring headers;
    headers += L"Access-Control-Allow-Origin: " +
        toWString(security.value("corsAllowOrigin", "*")) + L"\r\n";
    headers += L"Access-Control-Allow-Methods: " +
        toWString(security.value("corsAllowMethods",
            "GET, POST, PUT, DELETE, PATCH, OPTIONS")) + L"\r\n";
    headers += L"Access-Control-Allow-Headers: " +
        toWString(security.value("corsAllowHeaders", "*")) + L"\r\n";
    if (security.value("corsAllowCredentials", false)) {
        headers += L"Access-Control-Allow-Credentials: true\r\n";
    }
    headers += L"Access-Control-Max-Age: " +
        toWString(security.value("corsMaxAge", "86400")) + L"\r\n";

    // Add PNA response header if this is a PNA preflight
    if (isPnaPreflight) {
        headers += L"Access-Control-Allow-Private-Network: true\r\n";
    }

    headers += L"Content-Length: 0\r\n";

    // Create an empty stream for the response body
    Microsoft::WRL::ComPtr<IStream> emptyStream;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 1);
    if (hMem) {
        GlobalLock(hMem);
        GlobalUnlock(hMem);
        CreateStreamOnHGlobal(hMem, TRUE, &emptyStream);
    }

    // Create the 204 No Content response
    Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> response;
    env->CreateWebResourceResponse(
        emptyStream.Get(),
        204,
        L"No Content",
        headers.c_str(),
        &response
    );

    if (response) {
        args->put_Response(response.Get());
    }
}

// ============================================================================
//  Security Configuration — Request Proxy with Modified Headers
//  (CORS + CSP + PNA)
// ============================================================================

// Proxy an HTTP/HTTPS request using WinHTTP, with modified response headers
// (removes CSP headers and/or adds CORS/PNA headers)
//
// NOTE: This function uses synchronous WinHTTP and will block the calling
// thread (UI thread) until the HTTP request completes. For production use,
// consider using async WinHTTP with deferrals.
static bool proxyRequestWithModifiedHeaders(
    ICoreWebView2WebResourceRequestedEventArgs* args,
    ICoreWebView2Environment* env,
    const nlohmann::json& config,
    bool removeCsp,
    bool addCors,
    bool addPna)
{
    // Get the request object
    Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request;
    if (FAILED(args->get_Request(&request)) || !request)
        return false;

    // Get URL
    wil::unique_cotaskmem_string uriStr;
    if (FAILED(request->get_Uri(&uriStr)) || !uriStr)
        return false;
    std::wstring url(uriStr.get());

    // Get method
    wil::unique_cotaskmem_string methodStr;
    if (FAILED(request->get_Method(&methodStr)) || !methodStr)
        return false;
    std::wstring method(methodStr.get());

    // Get request headers
    std::wstring reqHeadersStr = getRequestHeadersString(request.Get());

    // Get request body
    Microsoft::WRL::ComPtr<IStream> contentStream;
    request->get_Content(&contentStream);
    std::vector<BYTE> requestBody = readIStream(contentStream.Get());

    // Parse URL
    std::wstring scheme, host, path;
    INTERNET_PORT port;
    bool isHttps;
    if (!parseUrl(url, scheme, host, port, path, isHttps))
        return false;

    // Create WinHTTP session
    HINTERNET hSession = WinHttpOpen(
        L"WebNative/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return false;

    // Set timeouts (5s connect, 30s send/receive)
    WinHttpSetTimeouts(hSession, 5000, 5000, 30000, 30000);

    // Connect to the host
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Create the request
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        method.c_str(),
        path.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // For HTTPS, ignore certificate errors (the --ignore-certificate-errors
    // flag handles this at the browser level, but WinHTTP also needs it)
    if (isHttps) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                              SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                         &securityFlags, sizeof(securityFlags));
    }

    // Add request headers (excluding Host and Content-Length, already handled)
    if (!reqHeadersStr.empty()) {
        WinHttpAddRequestHeaders(hRequest, reqHeadersStr.c_str(),
            (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // Send the request
    BOOL bResult = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        requestBody.empty() ? WINHTTP_NO_REQUEST_DATA : requestBody.data(),
        (DWORD)requestBody.size(),
        (DWORD)requestBody.size(),
        0
    );
    if (!bResult) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Receive the response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Get status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    // Get status text (reason phrase)
    std::wstring reasonPhrase;
    DWORD reasonSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT,
        WINHTTP_HEADER_NAME_BY_INDEX, NULL, &reasonSize, WINHTTP_NO_HEADER_INDEX);
    if (reasonSize > 0) {
        reasonPhrase.resize(reasonSize / sizeof(wchar_t));
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT,
            WINHTTP_HEADER_NAME_BY_INDEX, reasonPhrase.data(),
            &reasonSize, WINHTTP_NO_HEADER_INDEX);
        // Remove trailing null
        if (!reasonPhrase.empty() && reasonPhrase.back() == L'\0')
            reasonPhrase.pop_back();
    }

    // Get all response headers
    std::wstring allHeaders;
    DWORD headersSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS,
        WINHTTP_HEADER_NAME_BY_INDEX, NULL, &headersSize, WINHTTP_NO_HEADER_INDEX);
    if (headersSize > 0) {
        allHeaders.resize(headersSize / sizeof(wchar_t));
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS,
            WINHTTP_HEADER_NAME_BY_INDEX, allHeaders.data(),
            &headersSize, WINHTTP_NO_HEADER_INDEX);
    }

    // Read response body
    std::vector<BYTE> responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<BYTE> chunk(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.insert(responseBody.end(), chunk.begin(), chunk.begin() + bytesRead);
        } else {
            break;
        }
    }

    // Close WinHTTP handles
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Modify response headers (remove CSP, add CORS, add PNA)
    std::wstring modifiedHeaders = modifyResponseHeaders(
        allHeaders, removeCsp, addCors, addPna, config, responseBody
    );

    // Create IStream from response body
    Microsoft::WRL::ComPtr<IStream> responseStream = createStreamFromBuffer(responseBody);

    // Create WebView2 response
    Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> response;
    env->CreateWebResourceResponse(
        responseStream.Get(),
        (int)statusCode,
        reasonPhrase.c_str(),
        modifiedHeaders.c_str(),
        &response
    );

    if (response) {
        args->put_Response(response.Get());
        return true;
    }

    return false;
}

// ============================================================================
//  Security Configuration — CORS & CSP Interceptor Setup (with PNA)
// ============================================================================

// Set up WebResourceRequested handler for CORS, CSP, and PNA interception
static void setupCorsAndCspInterceptor(
    ICoreWebView2* webview,
    ICoreWebView2Environment* env,
    const nlohmann::json& config)
{
    auto security = config.value("security", nlohmann::json::object());
    auto features = config.value("features", nlohmann::json::object());
    auto pnaConfig = features.value("pna", nlohmann::json::object());

    bool disableCors  = security.value("disableCors", false);
    bool disableCsp   = security.value("disableCsp", false);
    bool pnaEnabled   = pnaConfig.value("enabled", false);
    bool pnaAddHeader = pnaConfig.value("addPrivateNetworkHeader", false);

    if (!disableCors && !disableCsp && !pnaEnabled)
        return;

    // Add filter for all web resources
    webview->AddWebResourceRequestedFilter(
        L"*",
        COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL
    );

    // Register the WebResourceRequested event handler
    webview->add_WebResourceRequested(
        Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [env, config, disableCors, disableCsp, pnaEnabled, pnaAddHeader](
                ICoreWebView2* sender,
                ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT
            {
                // Get the request
                Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request)
                    return S_OK;

                // Get the URL
                wil::unique_cotaskmem_string uriStr;
                if (FAILED(request->get_Uri(&uriStr)) || !uriStr)
                    return S_OK;
                std::wstring url(uriStr.get());

                // Only intercept HTTP/HTTPS requests
                if (!isHttpUrl(url))
                    return S_OK;

                // Check if the host is allowed
                if (!shouldInterceptUrl(url, config))
                    return S_OK;

                // Get the method
                wil::unique_cotaskmem_string methodStr;
                request->get_Method(&methodStr);
                std::wstring method(methodStr.get());

                // Handle CORS preflight (OPTIONS) requests — also handles PNA preflight
                if (disableCors && _wcsicmp(method.c_str(), L"OPTIONS") == 0) {
                    handleCorsPreflight(args, env, config);
                    return S_OK;
                }

                // For other requests, proxy with modified headers
                bool removeCsp = disableCsp;
                bool addCors   = disableCors;
                bool addPna    = pnaEnabled && pnaAddHeader;

                if (removeCsp || addCors || addPna) {
                    proxyRequestWithModifiedHeaders(
                        args, env, config, removeCsp, addCors, addPna
                    );
                }

                return S_OK;
            }
        ).Get(),
        nullptr
    );
}

// ============================================================================
//  Security Configuration — Certificate Error Handler
// ============================================================================

// Set up certificate error handler to ignore SSL/TLS certificate errors.
// Uses ICoreWebView2_14::add_ServerCertificateErrorDetected if available,
// otherwise relies on the --ignore-certificate-errors browser flag.
static void setupCertificateErrorHandler(ICoreWebView2* webview, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    if (!security.value("ignoreCertificateErrors", false))
        return;

    // Try to get ICoreWebView2_14 interface (SDK 1.0.1518.0+)
    Microsoft::WRL::ComPtr<ICoreWebView2_14> webview14;
    if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview14))) && webview14) {
        webview14->add_ServerCertificateErrorDetected(
            Microsoft::WRL::Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                [](ICoreWebView2* sender,
                   ICoreWebView2ServerCertificateErrorDetectedEventArgs* args) -> HRESULT
                {
                    // Always allow certificate errors
                    args->put_Action(
                        COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_ALWAYS_ALLOW
                    );
                    return S_OK;
                }
            ).Get(),
            nullptr
        );
    }
    // If ICoreWebView2_14 is not available, the --ignore-certificate-errors
    // browser flag (set in buildBrowserArguments) handles it.
}

// ============================================================================
//  Security Configuration — Permission Handler
// ============================================================================

// Map a config permission string ("allow", "deny", "default") to
// COREWEBVIEW2_PERMISSION_STATE
static COREWEBVIEW2_PERMISSION_STATE parsePermissionState(const std::string& stateStr) {
    if (stateStr == "allow")
        return COREWEBVIEW2_PERMISSION_STATE_ALLOW;
    if (stateStr == "deny")
        return COREWEBVIEW2_PERMISSION_STATE_DENY;
    return COREWEBVIEW2_PERMISSION_STATE_DEFAULT;
}

// Get the configured permission state for a given permission kind.
// Returns true if a config value was found, false otherwise.
static bool getConfiguredPermissionState(
    const nlohmann::json& config,
    COREWEBVIEW2_PERMISSION_KIND kind,
    COREWEBVIEW2_PERMISSION_STATE& outState)
{
    auto permissions = config.value("permissions", nlohmann::json::object());
    if (permissions.empty())
        return false;

    // If autoAllowAll is true, allow everything
    if (permissions.value("autoAllowAll", false)) {
        outState = COREWEBVIEW2_PERMISSION_STATE_ALLOW;
        return true;
    }

    // Map permission kind to config key
    std::string configKey;
    switch (kind) {
        case COREWEBVIEW2_PERMISSION_KIND_MICROPHONE:
            configKey = "microphone"; break;
        case COREWEBVIEW2_PERMISSION_KIND_CAMERA:
            configKey = "camera"; break;
        case COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION:
            configKey = "geolocation"; break;
        case COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS:
            configKey = "notifications"; break;
        case COREWEBVIEW2_PERMISSION_KIND_OTHER_PERMISSIONS:
            configKey = "multiple"; break;
        case COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ:
            configKey = "clipboardRead"; break;
        // ICoreWebView2PermissionRequestedEventArgs2 (SDK 1.0.1518.0+)
        // These may not be available in all SDK versions, but we handle them
        // via numeric comparison for forward compatibility
        default:
            // Handle extended permission kinds via numeric values
            // COREWEBVIEW2_PERMISSION_KIND_MULTIPLE_PERMISSIONS = 6
            // COREWEBVIEW2_PERMISSION_KIND_FILE_READ_WRITE = 8
            // COREWEBVIEW2_PERMISSION_KIND_AUTOPLAY = 9
            // COREWEBVIEW2_PERMISSION_KIND_LOCAL_FONTS = 10
            // COREWEBVIEW2_PERMISSION_KIND_MIDI_SYSEX = 11
            // COREWEBVIEW2_PERMISSION_KIND_WINDOW_MANAGEMENT = 12
            // COREWEBVIEW2_PERMISSION_KIND_PERSISTENT_DURABLE_STORAGE = 13
            // COREWEBVIEW2_PERMISSION_KIND_IDLE_DETECTION = 14
            // COREWEBVIEW2_PERMISSION_KIND_STORAGE_ACCESS = 15
            // COREWEBVIEW2_PERMISSION_KIND_DOCUMENT_PICTURE_IN_PICTURE = 16
            switch ((int)kind) {
                case 6:  configKey = "multiple"; break;
                case 8:  configKey = "fileReadWrite"; break;
                case 9:  configKey = "autoplay"; break;
                case 10: configKey = "localFonts"; break;
                case 11: configKey = "midiSysex"; break;
                case 12: configKey = "windowManagement"; break;
                case 13: configKey = "persistentDurableStorage"; break;
                case 14: configKey = "idleDetection"; break;
                case 15: configKey = "storageAccess"; break;
                case 16: configKey = "documentPictureInPicture"; break;
                default:
                    return false;
            }
            break;
    }

    if (!permissions.contains(configKey))
        return false;

    std::string stateStr = permissions.value(configKey, "default");
    outState = parsePermissionState(stateStr);
    return true;
}

// Set up the PermissionRequested event handler.
// This intercepts all permission requests from the web content and
// applies the configured permission state (allow/deny/default).
//
// For PNA (Private Network Access), the permission is handled through
// the WebResourceRequested event (preflight interception), not through
// PermissionRequested. However, some PNA-related permissions (like
// local network access) may also trigger PermissionRequested on newer
// Chromium versions.
static void setupPermissionHandler(ICoreWebView2* webview, const nlohmann::json& config) {
    auto permissions = config.value("permissions", nlohmann::json::object());
    if (permissions.empty())
        return;

    bool savesInProfile = permissions.value("savesInProfile", false);

    webview->add_PermissionRequested(
        Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [config, savesInProfile](
                ICoreWebView2* sender,
                ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT
            {
                COREWEBVIEW2_PERMISSION_KIND kind;
                args->get_PermissionKind(&kind);

                COREWEBVIEW2_PERMISSION_STATE state;
                if (getConfiguredPermissionState(config, kind, state)) {
                    args->put_State(state);
                    args->put_Handled(TRUE);

                    // Try to set SavesInProfile via ICoreWebView2PermissionRequestedEventArgs3
                    // (SDK 1.0.1518.0+)
                    if (!savesInProfile) {
                        Microsoft::WRL::ComPtr<ICoreWebView2PermissionRequestedEventArgs3> args3;
                        if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args3))) && args3) {
                            args3->put_SavesInProfile(FALSE);
                        }
                    }

                    return S_OK;
                }

                // If no config found, use default behavior
                return S_OK;
            }
        ).Get(),
        nullptr
    );
}

// ============================================================================
//  Security Configuration — PNA (Private Network Access) Additional Setup
// ============================================================================

// Set up additional PNA-related configurations that cannot be handled
// purely through browser flags or WebResourceRequested.
//
// PNA (formerly CORS-RFC1918) restricts requests from public contexts
// to private network resources. The flow is:
//
// 1. Public website makes a request to a private network address
// 2. Browser sends a CORS preflight with:
//    Access-Control-Request-Private-Network: true
// 3. Private network server must respond with:
//    Access-Control-Allow-Private-Network: true
// 4. If the server doesn't respond correctly, the request is blocked
//
// In WebView2, we handle this by:
// - Intercepting the preflight via WebResourceRequested and auto-responding
// - Adding the PNA header to proxied responses
// - Using browser flags to disable enforcement if needed
//
// References:
// https://wicg.github.io/private-network-access/
// https://github.com/WICG/private-network-access/blob/main/explainer.md
// https://github.com/WICG/private-network-access/blob/main/HOWTO.md
// https://developer.chrome.com/blog/private-network-access-update-2024-03

static void setupPnaConfiguration(ICoreWebView2* webview, const nlohmann::json& config) {
    auto features = config.value("features", nlohmann::json::object());
    auto pnaConfig = features.value("pna", nlohmann::json::object());

    if (!pnaConfig.value("enabled", false))
        return;

    // Inject a script that patches fetch/XHR to add PNA headers
    // This helps with requests that bypass the WebResourceRequested handler
    if (pnaConfig.value("addPrivateNetworkHeader", false)) {
        std::wstring pnaScript = LR"(
            (function() {
                'use strict';

                // Patch fetch to add PNA header for private network requests
                var originalFetch = window.fetch;
                if (originalFetch) {
                    window.fetch = function(input, init) {
                        init = init || {};
                        init.headers = init.headers || {};

                        // Convert Headers object to plain object if needed
                        var headers = init.headers;
                        if (headers instanceof Headers) {
                            var plainHeaders = {};
                            headers.forEach(function(value, key) {
                                plainHeaders[key] = value;
                            });
                            headers = plainHeaders;
                        }

                        // Add PNA header
                        headers['Access-Control-Request-Private-Network'] = 'true';

                        init.headers = headers;
                        return originalFetch.call(this, input, init);
                    };
                }

                // Patch XMLHttpRequest to add PNA header
                var originalOpen = XMLHttpRequest.prototype.open;
                var originalSend = XMLHttpRequest.prototype.send;

                XMLHttpRequest.prototype.open = function(method, url) {
                    this._pnaUrl = url;
                    return originalOpen.apply(this, arguments);
                };

                XMLHttpRequest.prototype.send = function() {
                    try {
                        this.setRequestHeader(
                            'Access-Control-Request-Private-Network', 'true'
                        );
                    } catch(e) {
                        // setRequestHeader may fail if headers are already sent
                    }
                    return originalSend.apply(this, arguments);
                };
            })();
        )";
        webview->AddScriptToExecuteOnDocumentCreated(pnaScript.c_str(), nullptr);
    }
}

// ============================================================================
//  Security Configuration — Apply All Settings
// ============================================================================

// Apply all security, permission, and feature settings to the webview
static void applySecurityConfig(
    ICoreWebView2* webview,
    ICoreWebView2Environment* env,
    const nlohmann::json& config)
{
    auto security = config.value("security", nlohmann::json::object());
    auto features = config.value("features", nlohmann::json::object());

    // 1. CSP meta tag remover
    if (security.value("disableCspMetaTags", false) ||
        security.value("disableCsp", false))
    {
        injectCspMetaRemover(webview);
    }

    // 2. CORS and CSP interceptor (also handles PNA preflight)
    if (security.value("disableCors", false) ||
        security.value("disableCsp", false) ||
        features.value("pna", nlohmann::json::object()).value("enabled", false))
    {
        setupCorsAndCspInterceptor(webview, env, config);
    }

    // 3. Certificate error handler
    if (security.value("ignoreCertificateErrors", false)) {
        setupCertificateErrorHandler(webview, config);
    }

    // 4. Permission handler
    setupPermissionHandler(webview, config);

    // 5. PNA additional configuration (script injection)
    setupPnaConfiguration(webview, config);
}

// ============================================================================
//  Main Application Functions
// ============================================================================

void runApplication(HINSTANCE hInstance, int nCmdShow) {
    auto& config = getConfig();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WebnativeWindow";
    RegisterClassW(&wc);

    Globals::hwnd = createWindow(hInstance, config);
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
    int width = windowConfig.value("width", 1200);
    int height = windowConfig.value("height", 700);
    std::string titleStr = config.value("name", "Webnative application");
    std::wstring title(titleStr.begin(), titleStr.end());

    HWND hwnd = CreateWindowExW(
        0, L"WebnativeWindow", title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, hInstance, NULL
    );

    applyConfigToWindow(hwnd, config);
    return hwnd;
}

void applyConfigToWindow(HWND hwnd, const nlohmann::json& config) {
    auto windowConfig = config.value("window", nlohmann::json::object());
    if (windowConfig.value("fullscreen", false)) ShowWindow(hwnd, SW_MAXIMIZE);

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int minWidth = windowConfig.value("minWidth", 500);
    int minHeight = windowConfig.value("minHeight", 700);
    SetWindowPos(hwnd, NULL, rect.left, rect.top,
        max(rect.right - rect.left, minWidth),
        max(rect.bottom - rect.top, minHeight),
        SWP_NOZORDER
    );
}

void createWebview(HWND hwnd, const nlohmann::json& config) {
    std::wstring appDir = toWString(config.value("appDir", "."));
    std::wstring dataDir = appDir + L"\\webview2_data";

    // Build browser arguments from security + features config
    std::wstring browserArgs = buildBrowserArguments(config);

    // Create environment options with browser arguments
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> envOptions =
        createEnvironmentOptions(browserArgs);

    // Create the WebView2 environment with options
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        dataDir.c_str(),
        envOptions.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, appDir](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                env->CreateCoreWebView2Controller(hwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, appDir, env](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                            Globals::controller = ctrl;
                            Globals::controller->get_CoreWebView2(&Globals::webview);

                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            Globals::controller->put_Bounds(bounds);

                            // Inject the postMessage bridge
                            Globals::webview->AddScriptToExecuteOnDocumentCreated(
                                L"window.sendSignalToCpp = (msg) => window.chrome.webview.postMessage(msg);",
                                nullptr
                            );

                            // Set up the WebMessageReceived handler
                            Globals::webview->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        onApiCall(sender, args);
                                        return S_OK;
                                    }
                                ).Get(), nullptr
                            );

                            // Apply webview settings (dev tools, etc.)
                            applyConfigToWebviewSettings(getConfig());

                            // Apply security, permissions, and features configuration
                            applySecurityConfig(Globals::webview, env, getConfig());

                            // Load the HTML content
                            loadHtmlToWebview(appDir);

                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );
}

void loadHtmlToWebview(const std::wstring& appDir) {
    std::wstring publicPath = L"file:///" + appDir + L"\\public\\index.html";
    std::replace(publicPath.begin(), publicPath.end(), L'\\', L'/');
    Globals::webview->Navigate(publicPath.c_str());
}

void applyConfigToWebviewSettings(const nlohmann::json& config) {
    ICoreWebView2Settings* settings;
    Globals::webview->get_Settings(&settings);

    if (config.value("env", "development") != "development") return;
    settings->put_AreDevToolsEnabled(TRUE);
    Globals::webview->OpenDevToolsWindow();
}

void onApiCall(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
    wil::unique_cotaskmem_string message;
    args->TryGetWebMessageAsString(&message);
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
                GetClientRect(hwnd, &bounds);
                Globals::controller->put_Bounds(bounds);
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