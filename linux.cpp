#include "linux.hpp"
#include <iostream>
#include <exception>

int main(int argc, char **argv) {
	try {
		setupPipe();
		forkNode();
		runApplication(argc, argv);
	}
	catch (const std::exception& e) {
		std::cerr << "Application error: " << e.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cerr << "Unknown application error" << std::endl;
		return 1;
	}
	
	terminate();
	return 0;
}

void terminate() {
	try {
		terminateNode();
	}
	catch (const std::exception& e) {
		std::cerr << "Error terminating Node: " << e.what() << std::endl;
	}
	
	try {
		terminateWebkitApp();
	}
	catch (const std::exception& e) {
		std::cerr << "Error terminating WebKit: " << e.what() << std::endl;
	}
}
