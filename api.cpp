#include "api.hpp"
#include "globals.hpp"
#include <stdexcept>

void handleApiCall(ICoreWebView2* webview, const std::string& data) {
	if (!webview) {
		return; // Cannot send response without webview pointer
	}
	
	try {
		auto payload = nlohmann::json::parse(data);
		std::string action = payload.value("action", "none");

		if (action == "none") return;
		if (action == "authenticate") authenticate();
	}
	catch (const nlohmann::json::exception& e) {
		// Log JSON parse error - don't throw to avoid crashing webview
	}
}

void authenticate() {
	if (!Globals::webview) {
		return; // No webview to send authentication data to
	}
	
	try {
		if (Globals::authentication.is_null()) {
			Globals::authentication = getFromNode();
		}
		sendToWebview(Globals::webview.get(), Globals::authentication);
	}
	catch (const std::exception& e) {
		// Handle authentication errors
	}
}
