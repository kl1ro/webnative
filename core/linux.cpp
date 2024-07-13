#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

namespace env {
	const bool dev = true;
	const int minWidth = 450;
	const int minHeight = 700;
	const char title[] = "Test";
	// Make sure this is unique
	const char id[] = "com.example.test";
}

int backPipe[2], forthPipe[2];
WebKitWebView *web_view;

gboolean sendSignal(const char *script) {
	webkit_web_view_run_javascript(web_view, script, NULL, NULL, NULL);
	return G_SOURCE_REMOVE;
}

static void getSignal(WebKitUserContentManager *manager, WebKitJavascriptResult *js_result, gpointer userData) {
	JSCValue *value = webkit_javascript_result_get_js_value(js_result);
	if(jsc_value_is_string(value)) {
		gchar* str_value = jsc_value_to_string(value);
		write(forthPipe[1], str_value, strlen(str_value));
		char temp[65536]; std::string message;
		ssize_t size = read(backPipe[0], temp, 65535);
		while(size == 65535) {
			temp[size] = 0;
			message += std::string(temp);
			write(forthPipe[1], "more", 4);
			size = read(backPipe[0], temp, 65535);
		}
		temp[size] = 0;
		message += std::string(temp);
		std::string script = "window.receiveSignalFromCpp(" + std::string(message) + ");";
		sendSignal(script.c_str());
		g_free(str_value);
	}
}

static void activate(GtkApplication *app, gpointer userData) {
	GtkWidget *window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), env::title);
	GdkGeometry hints;
	hints.min_width = env::minWidth;
	hints.min_height = env::minHeight;
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &hints, GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);
	WebKitUserContentManager *content_manager = webkit_user_content_manager_new();
	g_signal_connect(content_manager, "script-message-received::cppSignal", G_CALLBACK(getSignal), NULL);
	webkit_user_content_manager_register_script_message_handler(content_manager, "cppSignal");
	web_view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(content_manager));
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(web_view));
	GFile *file = g_file_new_for_path("./usr/bin/public/index.html");
	gchar *baseURL = g_file_get_uri(file);
	g_object_unref(file);
	webkit_web_view_load_uri(web_view, baseURL);
	g_free(baseURL);
	gtk_widget_show_all(window);

	if(env::dev) {
		WebKitSettings *settings = webkit_web_view_get_settings(web_view);
		g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);
		WebKitWebInspector *inspector = webkit_web_view_get_inspector(web_view);
		webkit_web_inspector_show(inspector);
	}
}

int main(int argc, char **argv) {
	if(pipe(backPipe) == -1 || pipe(forthPipe) == -1) return -1;
	int pid = fork();
	if(pid == -1) return -1;

	// Here the parent process continues
	if(pid) {
		GtkApplication *app = gtk_application_new(env::id, G_APPLICATION_DEFAULT_FLAGS);
		g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
		int status = g_application_run(G_APPLICATION(app), argc, argv);
		g_object_unref(app);
		return status;
	}

	// Start the nodejs child process
	execlp("node", "node", "usr/bin/backend/main.mjs", std::to_string(forthPipe[0]).c_str(), std::to_string(backPipe[1]).c_str(), nullptr);
	return 0;
}