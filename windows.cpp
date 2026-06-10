#include "windows.hpp"
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    try {
        setupPipe();
        forkNode();
        runApplication(hInstance, nCmdShow);
        terminate();
    }
    catch (const std::exception& e) {
        const char* errorMsg = e.what();
        std::cerr << "Application error: " << errorMsg << std::endl;
        MessageBoxA(NULL, errorMsg, "Application Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown application error" << std::endl;
        MessageBoxA(NULL, "An unknown error occurred", "Application Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}

void terminate() {
	try {
		terminateNode();
	}
	catch (const std::exception& e) {
		std::cerr << "Error terminating Node.js: " << e.what() << std::endl;
	}
	
	try {
		terminateWebviewApp();
	}
	catch (const std::exception& e) {
		std::cerr << "Error terminating WebView: " << e.what() << std::endl;
	}
}
