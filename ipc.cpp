#include "ipc.hpp"
#include "globals.hpp"
#include <iostream>
#include <stdexcept>

// Configuration constants
const DWORD PIPE_BUFFER_SIZE = 65536;
const DWORD PIPE_TIMEOUT = 30000; // 30 seconds

void setupPipe() {
    Globals::pipeName = getPipeName();

    Globals::pipe = CreateNamedPipeW(
        Globals::pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, PIPE_TIMEOUT, NULL
    );

    if (Globals::pipe == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        throw std::runtime_error("Failed to create named pipe. Error code: " + std::to_string(error));
    }
}

void sendToWebview(ICoreWebView2* webview, const nlohmann::json& object) {
    if (!webview) {
        return; // Cannot execute script without webview
    }
    
    try {
        std::string script = "window.receiveSignalFromCpp(" + object.dump() + ");";
        std::wstring wscript(script.begin(), script.end());
        
        HRESULT hr = webview->ExecuteScript(wscript.c_str(), nullptr);
        if (FAILED(hr)) {
            // Log script execution failure - don't throw
        }
    }
    catch (const std::exception& e) {
        // Handle JSON or conversion errors
    }
}

nlohmann::json getFromNode() {
    if (!Globals::pipe || Globals::pipe == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Pipe handle is invalid");
    }
    
    std::string data;
    data.resize(PIPE_BUFFER_SIZE);
    
    DWORD bytesRead = 0;
    if (!ReadFile(Globals::pipe, &data[0], PIPE_BUFFER_SIZE - 1, &bytesRead, NULL)) {
        DWORD error = GetLastError();
        throw std::runtime_error("Failed to read from pipe. Error code: " + std::to_string(error));
    }
    
    if (bytesRead == 0) {
        throw std::runtime_error("No data received from pipe");
    }
    
    data.resize(bytesRead);
    try {
        return nlohmann::json::parse(data);
    }
    catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse JSON from pipe: " + std::string(e.what()));
    }
}

inline std::wstring getPipeName() {
    return L"\\\\.\\pipe\\webnative-" + std::to_wstring(GetCurrentProcessId());
}
