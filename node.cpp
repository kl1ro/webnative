#include "node.hpp"
#include "globals.hpp"
#include <stdexcept>
#include <filesystem>

void forkNode() {
    auto& config = getConfig();

    if (config["nodePath"].is_null() || !config["nodePath"].is_string()) {
        std::cerr << "Node.js not found, starting in frontend-only mode" << std::endl;
        return;
    }

    try {
        std::wstring appDir = toWString(config.value("appDir", "."));
        std::wstring backendPath = appDir + L"\\backend\\index.js";

        if (!std::filesystem::exists(backendPath)) {
            std::cerr << "Backend not found, starting in frontend-only mode" << std::endl;
            return;
        }

        std::wstring nodePath = toWString(config["nodePath"].get<std::string>());
        
        if (!std::filesystem::exists(nodePath)) {
            std::cerr << "Node.js executable not found at: " << std::string(nodePath.begin(), nodePath.end()) << std::endl;
            return;
        }
        
        // Construct command line with quotes
        std::wstring cmdLine = L"\"" + nodePath + L"\" \"" + backendPath + L"\" " + Globals::pipeName;

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            DWORD error = GetLastError();
            std::cerr << "Failed to start backend process. Error code: " << error << std::endl;
            return; // Non-fatal: allow frontend-only mode
        }

        Globals::nodePid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess); // Also close process handle
    }
    catch (const std::exception& e) {
        std::cerr << "Error starting Node.js: " << e.what() << std::endl;
    }
}

void terminateNode() {
    if (Globals::nodePid == 0) return;
    
    try {
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, Globals::nodePid);
        if (!process) {
            return; // Process may have already terminated
        }
        
        if (!TerminateProcess(process, 0)) {
            DWORD error = GetLastError();
            std::cerr << "Failed to terminate Node process. Error: " << error << std::endl;
        }
        
        CloseHandle(process);
    }
    catch (const std::exception& e) {
        std::cerr << "Error terminating Node process: " << e.what() << std::endl;
    }
}
