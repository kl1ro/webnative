#include "node.hpp"
#include "globals.hpp"

void forkNode() {
    auto& config = getConfig();

    if (config["nodePath"].is_null()) {
        std::cerr << "Node.js not found, starting in frontend-only mode" << std::endl;
        return;
    }

    std::wstring appDir = toWString(config.value("appDir", "."));
    std::wstring backendPath = appDir + L"\\backend\\index.js";

    if (GetFileAttributesW(backendPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Backend not found, starting in frontend-only mode" << std::endl;
        return;
    }

    std::wstring nodePath = toWString(config["nodePath"].get<std::string>());
    std::wstring cmdLine = L"\"" + nodePath + L"\" \"" + backendPath + L"\" " + Globals::pipeName;

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start backend" << std::endl;
        std::exit(1);
    }

    Globals::nodePid = pi.dwProcessId;
    CloseHandle(pi.hThread);
}

void terminateNode() {
    if (Globals::nodePid == 0) return;
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, Globals::nodePid);
    if (!process) return;
    TerminateProcess(process, 0);
    CloseHandle(process);
}
