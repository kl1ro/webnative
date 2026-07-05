#include "api.hpp"
#include "globals.hpp"

void handleApiCall(ICoreWebView2* webview, const std::string& data) {
	auto payload = nlohmann::json::parse(data);
	std::string action = payload.value("action", "none");

	if (action == "none") return;
	if (action == "authenticate") authenticate();
}

void authenticate() {
	if (Globals::authentication.is_null()) Globals::authentication = getFromNode();
	sendToWebview(Globals::webview.get(), Globals::authentication);
}
