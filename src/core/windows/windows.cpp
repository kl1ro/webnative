#include "windows.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    try {
        setupPipe();
        forkNode();
        runApplication(hInstance, nCmdShow);
        terminate();
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
    }
    return 0;
}

void terminate() {
	terminateNode();
	terminateWebviewApp();
}
