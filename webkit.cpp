#include "webkit.hpp"
#include "globals.hpp"
#include <stdexcept>
#include <filesystem>

// Configuration constants
const int DEFAULT_WINDOW_WIDTH = 1200;
const int DEFAULT_WINDOW_HEIGHT = 700;
const int DEFAULT_MIN_WIDTH = 500;
const int DEFAULT_MIN_HEIGHT = 700;
const char* PUBLIC_HTML_PATH = "/usr/bin/public/";

void runApplication(int argc, char **argv) {
	auto& config = getConfig();
	auto& app = Globals::app;

	app = gtk_application_new(
		config.value("id", "org.webnative.example").c_str(),
		G_APPLICATION_DEFAULT_FLAGS
	);
	
	if (!app) {
		throw std::runtime_error("Failed to create GTK application");
	}

	g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
	Globals::exitStatus = g_application_run(G_APPLICATION(app), argc, argv);
}

void activate(GtkApplication *app) {
	if (!app) return;
	
	try {
		Globals::webview = createWebviewWindow(app, Globals::config);
		if (!Globals::webview) {
			return;
		}

		if (!Globals::contentManager) {
			return;
		}
		
		webkit_user_content_manager_register_script_message_handler(
			Globals::contentManager, "webnative", NULL
		);

		g_signal_connect(
			Globals::contentManager,
			"script-message-received::webnative",
			G_CALLBACK(onApiCall),
			NULL
		);
	}
	catch (const std::exception& e) {
		std::cerr << "Error during application activation: " << e.what() << std::endl;
	}
}

WebKitWebView* createWebviewWindow(GtkApplication *app, const nlohmann::json& config) {
	if (!app) return nullptr;
	
	auto window = gtk_application_window_new(app);
	if (!window) {
		throw std::runtime_error("Failed to create application window");
	}
	
	applyConfigToWindow(window, config);

	auto webview = createWebview();
	if (!webview) {
		g_object_unref(window);
		throw std::runtime_error("Failed to create WebView");
	}
	
	gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(webview));
	loadHtmlToWebview(webview, "index.html");

	applyConfigToWebkitSettings(webview, config);
	gtk_window_present(GTK_WINDOW(window));
	return webview;
}

WebKitWebView* createWebview() {
	Globals::contentManager = webkit_user_content_manager_new();
	if (!Globals::contentManager) {
		return nullptr;
	}

	WebKitWebView* webview = WEBKIT_WEB_VIEW(
		g_object_new(WEBKIT_TYPE_WEB_VIEW, "user-content-manager", Globals::contentManager, NULL)
	);

	if (!webview) {
		g_object_unref(Globals::contentManager);
		Globals::contentManager = nullptr;
		return nullptr;
	}
	
	return webview;
}

void applyConfigToWindow(GtkWidget* window, const nlohmann::json& config) {
	if (!window) return;
	
	gtk_window_set_title(GTK_WINDOW(window), config.value("name", "Webnative application").c_str());

	auto windowConfig = config.value("window", nlohmann::json::object());
	if (windowConfig.value("fullscreen", false)) {
		gtk_window_fullscreen(GTK_WINDOW(window));
	}

	int minWidth = windowConfig.value("minWidth", DEFAULT_MIN_WIDTH);
	int minHeight = windowConfig.value("minHeight", DEFAULT_MIN_HEIGHT);
	gtk_widget_set_size_request(window, minWidth, minHeight);

	int width = windowConfig.value("width", DEFAULT_WINDOW_WIDTH);
	int height = windowConfig.value("height", DEFAULT_WINDOW_HEIGHT);
	gtk_window_set_default_size(GTK_WINDOW(window), width, height);
}

void loadHtmlToWebview(WebKitWebView* webview, const std::string& html) {
	if (!webview) return;
	
	if (html.empty()) {
		std::cerr << "HTML file name is empty" << std::endl;
		return;
	}
	
	try {
		std::string publicPath = Globals::config.value("appDir", ".") + PUBLIC_HTML_PATH + html;
		
		if (!std::filesystem::exists(publicPath)) {
			std::cerr << "HTML file not found: " << publicPath << std::endl;
			return;
		}
		
		GFile *file = g_file_new_for_path(publicPath.c_str());
		if (!file) {
			std::cerr << "Failed to create GFile for: " << publicPath << std::endl;
			return;
		}
		
		gchar *baseURL = g_file_get_uri(file);
		if (baseURL) {
			webkit_web_view_load_uri(webview, baseURL);
			g_free(baseURL);
		}
		
		g_object_unref(file);
	}
	catch (const std::exception& e) {
		std::cerr << "Error loading HTML: " << e.what() << std::endl;
	}
}

void applyConfigToWebkitSettings(WebKitWebView* webview, const nlohmann::json& config) {
	if (!webview) return;
	
	try {
		WebKitSettings *settings = webkit_web_view_get_settings(webview);
		if (!settings) {
			return;
		}
		
		webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE);
		webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);

		if (config.value("env", "development") != "development") return;
		
		webkit_settings_set_enable_developer_extras(settings, TRUE);
		WebKitWebInspector *inspector = webkit_web_view_get_inspector(webview);
		if (inspector) {
			webkit_web_inspector_show(inspector);
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error applying WebKit settings: " << e.what() << std::endl;
	}
}

void onApiCall(WebKitUserContentManager *manager, JSCValue *value) {
	if (!manager || !value) {
		return;
	}
	
	gchar *gstring = jsc_value_to_string(value);
	if (!gstring) {
		return;
	}
	
	try {
		std::string data(gstring);
		g_free(gstring);
		handleApiCall(manager, data);
	}
	catch (const std::exception& e) {
		std::cerr << "Error handling API call: " << e.what() << std::endl;
		g_free(gstring);
	}
}

void terminateWebkitApp() {
	if (Globals::app) {
		g_object_unref(Globals::app);
		Globals::app = nullptr;
	}
	std::exit(Globals::exitStatus);
}
