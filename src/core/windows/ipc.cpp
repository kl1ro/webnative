#include "ipc.hpp"
#include "globals.hpp"
#include <iostream>

void setupPipe() {
    Globals::pipeName = getPipeName();

    Globals::pipe = CreateNamedPipeW(
        Globals::pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 65536, 65536, 0, NULL
    );

    if (Globals::pipe == INVALID_HANDLE_VALUE) std::exit(1);
}

void sendToWebview(ICoreWebView2* webview, const nlohmann::json& object) {
    std::string script = "window.receiveSignalFromCpp(" + object.dump() + ");";
    std::wstring wscript(script.begin(), script.end());
    webview->ExecuteScript(wscript.c_str(), nullptr);
}

nlohmann::json getFromNode() {
    char data[65536];
    ReadFile(Globals::pipe, data, 65535, NULL, NULL);
    return nlohmann::json::parse(data);
}

inline std::wstring getPipeName() {
    return L"\\\\.\\pipe\\webnative-" + std::to_wstring(GetCurrentProcessId());
}
