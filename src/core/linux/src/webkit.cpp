#include "webkit.hpp"
#include "globals.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>

// ============================================================================
//  Forward declarations
// ============================================================================

void runApplication(int argc, char **argv);
void activate(GtkApplication *app);
WebKitWebView* createWebviewWindow(GtkApplication *app, const nlohmann::json& config);
WebKitWebView* createWebview();
void applyConfigToWindow(GtkWidget* window, const nlohmann::json& config);
void loadHtmlToWebview(WebKitWebView* webview, const std::string& html);
void applyConfigToWebkitSettings(WebKitWebView* webview, const nlohmann::json& config);
void onApiCall(WebKitUserContentManager *manager, JSCValue *value);
void terminateWebkitApp();
void handleApiCall(WebKitUserContentManager *manager, const std::string& data);

// Security
static void setupWebContext(const nlohmann::json& config);
static void applySecurityConfig(WebKitWebView* webview, const nlohmann::json& config);
static void setupSecurityManager(WebKitWebContext* context, const nlohmann::json& config);
static void setupCertificateErrorHandler(WebKitWebContext* context, const nlohmann::json& config);
static void injectCspMetaRemover(WebKitUserContentManager* manager);
static void injectCorsBypassScript(WebKitUserContentManager* manager, const nlohmann::json& config);
static void setupHostAllowlist(WebKitWebView* webview, const nlohmann::json& config);
static void setupPnaConfiguration(WebKitUserContentManager* manager, const nlohmann::json& config);
static void injectAutomationFlagRemover(WebKitUserContentManager* manager);
static void injectSharedArrayBufferEnabler(WebKitUserContentManager* manager);
static void injectWebSecurityBypassScript(WebKitUserContentManager* manager);

// Permissions
static void setupPermissionHandler(WebKitWebView* webview, const nlohmann::json& config);

// Features
static void applyFeaturesConfig(WebKitWebView* webview, const nlohmann::json& config);

// Helpers
static std::string toLowerStr(const std::string& str);
static bool isHttpUrl(const std::string& url);
static std::string extractHostFromUrl(const std::string& url);
static bool isHostAllowed(const std::string& host, const nlohmann::json& config);
static void injectUserScript(WebKitUserContentManager* manager, const std::string& script);

// ============================================================================
//  Helper functions
// ============================================================================

static std::string toLowerStr(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static bool isHttpUrl(const std::string& url) {
    std::string lower = toLowerStr(url);
    return lower.find("http://") == 0 || lower.find("https://") == 0;
}

static std::string extractHostFromUrl(const std::string& url) {
    std::string result = url;
    size_t schemeEnd = result.find("://");
    if (schemeEnd == std::string::npos) return "";
    result = result.substr(schemeEnd + 3);
    size_t pathStart = result.find('/');
    if (pathStart != std::string::npos) result = result.substr(0, pathStart);
    size_t portStart = result.find(':');
    if (portStart != std::string::npos) result = result.substr(0, portStart);
    return result;
}

static bool isHostAllowed(const std::string& host, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    if (security.value("allowAllHosts", false)) return true;
    auto allowedHosts = security.value("allowedHosts", nlohmann::json::array());
    std::string hostLower = toLowerStr(host);
    for (const auto& h : allowedHosts) {
        if (toLowerStr(h.get<std::string>()) == hostLower) return true;
    }
    return false;
}

static void injectUserScript(WebKitUserContentManager* manager, const std::string& script) {
    WebKitUserScript* userScript = webkit_user_script_new(
        script.c_str(),
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr, nullptr
    );
    webkit_user_content_manager_add_script(manager, userScript);
    webkit_user_script_unref(userScript);
}

// ============================================================================
//  Web Context Setup
// ============================================================================

static void setupWebContext(const nlohmann::json& config) {
    WebKitWebContext* context = webkit_web_context_get_default();
    auto security = config.value("security", nlohmann::json::object());

    // TLS error policy
    if (security.value("ignoreCertificateErrors", false)) {
        webkit_web_context_set_tls_errors_policy(
            context, WEBKIT_TLS_ERRORS_POLICY_IGNORE
        );
    }

    // Security manager (scheme registration)
    setupSecurityManager(context, config);

    // Process model — shared process for all webviews
    webkit_web_context_set_process_model(
        context, WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES
    );

    // Sandbox: WebKitGTK uses bubblewrap sandbox by default.
    // noSandbox: can be disabled via WEBKIT_DISABLE_BUBBLEWRAP_SANDBOX env var
    if (security.value("noSandbox", false)) {
        g_setenv("WEBKIT_DISABLE_BUBBLEWRAP_SANDBOX", "1", TRUE);
    }
}

// ============================================================================
//  Security Configuration — Security Manager
// ============================================================================

static void setupSecurityManager(WebKitWebContext* context, const nlohmann::json& config) {
    WebKitSecurityManager* sm = webkit_web_context_get_security_manager(context);
    auto security = config.value("security", nlohmann::json::object());

    // disableWebSecurity: register http/https as local → bypasses same-origin policy
    if (security.value("disableWebSecurity", false)) {
        webkit_security_manager_register_uri_scheme_as_local(sm, "http");
        webkit_security_manager_register_uri_scheme_as_local(sm, "https");
    }

    // disableCors: register http/https as local → CORS not enforced for local schemes
    if (security.value("disableCors", false)) {
        webkit_security_manager_register_uri_scheme_as_local(sm, "http");
        webkit_security_manager_register_uri_scheme_as_local(sm, "https");
    }

    // allowInsecureContent: register http as secure → allows mixed content
    if (security.value("allowInsecureContent", false)) {
        webkit_security_manager_register_uri_scheme_as_secure(sm, "http");
    }

    // markOriginsAsSecure: register schemes as secure
    // NOTE: WebKitSecurityManager works at scheme level, not per-origin.
    // We register http/https as secure if any origins are specified.
    auto secureOrigins = security.value("markOriginsAsSecure", nlohmann::json::array());
    if (!secureOrigins.empty()) {
        webkit_security_manager_register_uri_scheme_as_secure(sm, "http");
        webkit_security_manager_register_uri_scheme_as_secure(sm, "https");
    }

    // allowInsecureLocalhost: localhost is already secure in recent WebKitGTK,
    // but we register http as secure to cover older versions
    if (security.value("allowInsecureLocalhost", false)) {
        webkit_security_manager_register_uri_scheme_as_secure(sm, "http");
    }

    // allowPrivateNetworkRequests: register private network schemes as local
    if (security.value("allowPrivateNetworkRequests", false)) {
        webkit_security_manager_register_uri_scheme_as_local(sm, "http");
        webkit_security_manager_register_uri_scheme_as_local(sm, "https");
    }
}

// ============================================================================
//  Security Configuration — Certificate Error Handler
// ============================================================================

static void setupCertificateErrorHandler(WebKitWebContext* context, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    if (!security.value("ignoreCertificateErrors", false)) return;

    // Set TLS errors policy to ignore at context level
    webkit_web_context_set_tls_errors_policy(
        context, WEBKIT_TLS_ERRORS_POLICY_IGNORE
    );
}

// ============================================================================
//  Security Configuration — CSP Meta Tag Removal (Script Injection)
// ============================================================================

static void injectCspMetaRemover(WebKitUserContentManager* manager) {
    std::string script = R"(
        (function() {
            'use strict';
            var removeCspMeta = function() {
                var selectors = [
                    'meta[http-equiv="Content-Security-Policy"]',
                    'meta[http-equiv="Content-Security-Policy-Report-Only"]',
                    'meta[http-equiv="X-Content-Security-Policy"]',
                    'meta[http-equiv="X-WebKit-CSP"]'
                ];
                selectors.forEach(function(sel) {
                    document.querySelectorAll(sel).forEach(function(el) { el.remove(); });
                });
            };
            removeCspMeta();
            if (document.documentElement) {
                var observer = new MutationObserver(function() { removeCspMeta(); });
                observer.observe(document.documentElement, {
                    childList: true, subtree: true
                });
            }
        })();
    )";
    injectUserScript(manager, script);
}

// ============================================================================
//  Security Configuration — CORS Bypass (Script Injection)
// ============================================================================

static void injectCorsBypassScript(WebKitUserContentManager* manager, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());

    std::string corsAllowOrigin = security.value("corsAllowOrigin", "*");
    std::string corsAllowMethods = security.value("corsAllowMethods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
    std::string corsAllowHeaders = security.value("corsAllowHeaders", "*");
    bool corsAllowCredentials = security.value("corsAllowCredentials", false);
    std::string corsMaxAge = security.value("corsMaxAge", "86400");

    std::string credsStr = corsAllowCredentials ? "true" : "false";

    std::string script = R"(
        (function() {
            'use strict';
            var corsAllowOrigin = ')" + corsAllowOrigin + R"(';
            var corsAllowMethods = ')" + corsAllowMethods + R"(';
            var corsAllowHeaders = ')" + corsAllowHeaders + R"(';
            var corsAllowCredentials = )" + credsStr + R"();
            var corsMaxAge = ')" + corsMaxAge + R"(';

            // Patch fetch to handle OPTIONS preflight locally
            var originalFetch = window.fetch;
            if (originalFetch) {
                window.fetch = function(input, init) {
                    init = init || {};
                    var method = (init.method || 'GET').toUpperCase();
                    if (method === 'OPTIONS') {
                        var hdrs = {
                            'Access-Control-Allow-Origin': corsAllowOrigin,
                            'Access-Control-Allow-Methods': corsAllowMethods,
                            'Access-Control-Allow-Headers': corsAllowHeaders,
                            'Access-Control-Max-Age': corsMaxAge
                        };
                        if (corsAllowCredentials) {
                            hdrs['Access-Control-Allow-Credentials'] = 'true';
                        }
                        return Promise.resolve(new Response('', {
                            status: 204,
                            statusText: 'No Content',
                            headers: new Headers(hdrs)
                        }));
                    }
                    return originalFetch.apply(this, arguments);
                };
            }

            // Patch XMLHttpRequest to intercept OPTIONS
            var originalXhrOpen = XMLHttpRequest.prototype.open;
            var originalXhrSend = XMLHttpRequest.prototype.send;

            XMLHttpRequest.prototype.open = function(method, url) {
                this._corsMethod = (method || 'GET').toUpperCase();
                return originalXhrOpen.apply(this, arguments);
            };

            XMLHttpRequest.prototype.send = function() {
                if (this._corsMethod === 'OPTIONS') {
                    var self = this;
                    setTimeout(function() {
                        try {
                            Object.defineProperty(self, 'status', {value: 204, writable: true});
                            Object.defineProperty(self, 'statusText', {value: 'No Content', writable: true});
                            Object.defineProperty(self, 'readyState', {value: 4, writable: true});
                            var getResponseHeader = self.getResponseHeader;
                            self.getResponseHeader = function(name) {
                                var lower = name.toLowerCase();
                                if (lower === 'access-control-allow-origin') return corsAllowOrigin;
                                if (lower === 'access-control-allow-methods') return corsAllowMethods;
                                if (lower === 'access-control-allow-headers') return corsAllowHeaders;
                                if (lower === 'access-control-max-age') return corsMaxAge;
                                if (lower === 'access-control-allow-credentials' && corsAllowCredentials) return 'true';
                                return null;
                            };
                            if (typeof self.onreadystatechange === 'function') self.onreadystatechange();
                            self.dispatchEvent(new Event('load'));
                            self.dispatchEvent(new Event('loadend'));
                        } catch(e) {}
                    }, 0);
                    return;
                }
                return originalXhrSend.apply(this, arguments);
            };
        })();
    )";

    injectUserScript(manager, script);
}

// ============================================================================
//  Security Configuration — Web Security Bypass Script
// ============================================================================

static void injectWebSecurityBypassScript(WebKitUserContentManager* manager) {
    std::string script = R"(
        (function() {
            'use strict';
            // Override isSecureContext for insecure origins
            try {
                if (!window.isSecureContext) {
                    Object.defineProperty(window, 'isSecureContext', {value: true});
                }
            } catch(e) {}
        })();
    )";
    injectUserScript(manager, script);
}

// ============================================================================
//  Security Configuration — Host Allowlist (Navigation Policy)
// ============================================================================

static void setupHostAllowlist(WebKitWebView* webview, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    if (security.value("allowAllHosts", true)) return;

    auto allowedHosts = security.value("allowedHosts", nlohmann::json::array());
    if (allowedHosts.empty()) return;

    // Store config as GRef'd copy for callback access
    nlohmann::json* configPtr = new nlohmann::json(config);

    g_signal_connect_data(webview, "decide-policy",
        G_CALLBACK(+[](WebKitWebView* webview, WebKitPolicyDecision* decision,
                       WebKitPolicyDecisionType type, gpointer userData) -> gboolean {
            auto* cfg = static_cast<nlohmann::json*>(userData);

            if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
                webkit_policy_decision_use(decision);
                return FALSE;
            }

            WebKitNavigationAction* action =
                webkit_navigation_policy_decision_get_navigation_action(
                    WEBKIT_NAVIGATION_POLICY_DECISION(decision));
            WebKitURIRequest* request = webkit_navigation_action_get_request(action);
            const gchar* uri = webkit_uri_request_get_uri(request);

            if (!uri || !isHttpUrl(uri)) {
                webkit_policy_decision_use(decision);
                return FALSE;
            }

            std::string host = extractHostFromUrl(uri);
            if (isHostAllowed(host, *cfg)) {
                webkit_policy_decision_use(decision);
            } else {
                webkit_policy_decision_ignore(decision);
            }
            return TRUE;
        }),
        configPtr,
        +[](gpointer data, GClosure*) {
            delete static_cast<nlohmann::json*>(data);
        },
        (GConnectFlags)0
    );
}

// ============================================================================
//  Security Configuration — PNA (Private Network Access)
// ============================================================================

static void setupPnaConfiguration(WebKitUserContentManager* manager, const nlohmann::json& config) {
    auto features = config.value("features", nlohmann::json::object());
    auto pnaConfig = features.value("pna", nlohmann::json::object());

    if (!pnaConfig.value("enabled", false)) return;

    // Inject PNA header patching script
    if (pnaConfig.value("addPrivateNetworkHeader", false)) {
        std::string pnaScript = R"(
            (function() {
                'use strict';
                var originalFetch = window.fetch;
                if (originalFetch) {
                    window.fetch = function(input, init) {
                        init = init || {};
                        init.headers = init.headers || {};
                        if (init.headers instanceof Headers) {
                            try {
                                init.headers.set('Access-Control-Request-Private-Network', 'true');
                            } catch(e) {}
                        } else {
                            init.headers['Access-Control-Request-Private-Network'] = 'true';
                        }
                        return originalFetch.call(this, input, init);
                    };
                }
                var originalXhrOpen = XMLHttpRequest.prototype.open;
                var originalXhrSend = XMLHttpRequest.prototype.send;
                XMLHttpRequest.prototype.open = function(method, url) {
                    this._pnaUrl = url;
                    return originalXhrOpen.apply(this, arguments);
                };
                XMLHttpRequest.prototype.send = function() {
                    try {
                        this.setRequestHeader('Access-Control-Request-Private-Network', 'true');
                    } catch(e) {}
                    return originalXhrSend.apply(this, arguments);
                };
            })();
        )";
        injectUserScript(manager, pnaScript);
    }

    // autoAllowPreflight: handled by CORS bypass script which intercepts OPTIONS
    // ignoreWorkerErrors / ignoreNavigationErrors: no direct WebKitGTK equivalent
}

// ============================================================================
//  Security Configuration — Automation Flag Remover
// ============================================================================

static void injectAutomationFlagRemover(WebKitUserContentManager* manager) {
    std::string script = R"(
        (function() {
            'use strict';
            try {
                Object.defineProperty(navigator, 'webdriver', {get: function() { return false; }});
            } catch(e) {}
        })();
    )";
    injectUserScript(manager, script);
}

// ============================================================================
//  Security Configuration — SharedArrayBuffer Enabler
// ============================================================================

static void injectSharedArrayBufferEnabler(WebKitUserContentManager* manager) {
    // WebKit requires cross-origin isolation (COOP/COEP) for SharedArrayBuffer.
    // Since we can't set HTTP response headers from the app side in WebKitGTK,
    // we inject a script that provides a polyfill if SharedArrayBuffer is not available.
    std::string script = R"(
        (function() {
            'use strict';
            if (typeof SharedArrayBuffer === 'undefined') {
                // SharedArrayBuffer not available; cross-origin isolation required.
                // Attempt to enable via crossOriginIsolated polyfill (limited effectiveness).
                try {
                    Object.defineProperty(window, 'crossOriginIsolated', {value: true});
                } catch(e) {}
            }
        })();
    )";
    injectUserScript(manager, script);
}

// ============================================================================
//  Security Configuration — Apply All
// ============================================================================

static void applySecurityConfig(WebKitWebView* webview, const nlohmann::json& config) {
    auto security = config.value("security", nlohmann::json::object());
    auto features = config.value("features", nlohmann::json::object());
    WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(webview);
    WebKitWebContext* context = webkit_web_view_get_context(webview);

    // 1. Certificate error handler
    setupCertificateErrorHandler(context, config);

    // 2. CSP meta tag remover
    if (security.value("disableCspMetaTags", false) ||
        security.value("disableCsp", false))
    {
        injectCspMetaRemover(manager);
    }

    // 3. CORS bypass
    if (security.value("disableCors", false)) {
        injectCorsBypassScript(manager, config);
    }

    // 4. Web security bypass
    if (security.value("disableWebSecurity", false)) {
        injectWebSecurityBypassScript(manager);
    }

    // 5. Host allowlist
    setupHostAllowlist(webview, config);

    // 6. PNA configuration
    setupPnaConfiguration(manager, config);

    // 7. Automation flag remover
    if (security.value("disableAutomationFlag", false)) {
        injectAutomationFlagRemover(manager);
    }

    // 8. SharedArrayBuffer enabler
    if (security.value("enableSharedArrayBuffer", false)) {
        injectSharedArrayBufferEnabler(manager);
    }

    // 9. Notifications: if disabled, deny all notification permissions
    //    (handled in permission handler)

    // NOTE: The following options have no direct WebKitGTK equivalent:
    // - disableStrictIsolation (no site-per-process in WebKitGTK)
    // - disableBackgroundThrottling (no background tab throttling)
    // - disableExtensions (no extension system in WebKitGTK)
    // - customFlags (Chromium-specific command-line flags)
    // - experimentalPlatformApi (use features.experimentalWebPlatformFeatures instead)
}

// ============================================================================
//  Permission Handler
// ============================================================================

enum class PermissionAction { Allow, Deny, Default };

static PermissionAction parsePermissionAction(const std::string& stateStr) {
    if (stateStr == "allow") return PermissionAction::Allow;
    if (stateStr == "deny") return PermissionAction::Deny;
    return PermissionAction::Default;
}

static PermissionAction getConfiguredPermission(
    const nlohmann::json& config, const std::string& configKey)
{
    auto permissions = config.value("permissions", nlohmann::json::object());
    if (permissions.empty()) return PermissionAction::Default;
    if (permissions.value("autoAllowAll", false)) return PermissionAction::Allow;
    if (!permissions.contains(configKey)) return PermissionAction::Default;
    return parsePermissionAction(permissions.value(configKey, "default"));
}

static void setupPermissionHandler(WebKitWebView* webview, const nlohmann::json& config) {
    auto permissions = config.value("permissions", nlohmann::json::object());
    auto security = config.value("security", nlohmann::json::object());
    if (permissions.empty() && !security.value("disableNotifications", false)) return;

    bool autoAllowAll = permissions.value("autoAllowAll", false);
    bool disableNotifications = security.value("disableNotifications", false);

    nlohmann::json* configPtr = new nlohmann::json(config);

    g_signal_connect_data(webview, "permission-request",
        G_CALLBACK(+[](WebKitWebView* webview, WebKitPermissionRequest* request,
                       gpointer userData) -> gboolean {
            auto* cfg = static_cast<nlohmann::json*>(userData);
            auto perms = cfg->value("permissions", nlohmann::json::object());
            bool autoAllow = perms.value("autoAllowAll", false);
            bool disNotif = cfg->value("security", nlohmann::json::object())
                                .value("disableNotifications", false);

            // Geolocation
            if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
                auto action = getConfiguredPermission(*cfg, "geolocation");
                if (action == PermissionAction::Allow || autoAllow) {
                    webkit_permission_request_allow(request);
                } else if (action == PermissionAction::Deny) {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // Notifications
            if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
                if (disNotif) {
                    webkit_permission_request_deny(request);
                    return TRUE;
                }
                auto action = getConfiguredPermission(*cfg, "notifications");
                if (action == PermissionAction::Allow || autoAllow) {
                    webkit_permission_request_allow(request);
                } else if (action == PermissionAction::Deny) {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // User media (microphone / camera)
            if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
                WebKitUserMediaPermissionRequest* mediaReq =
                    WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);
                bool isAudio = webkit_user_media_permission_is_for_audio_device(mediaReq);
                bool isVideo = webkit_user_media_permission_is_for_video_device(mediaReq);

                if (autoAllow) {
                    webkit_permission_request_allow(request);
                    return TRUE;
                }

                PermissionAction action = PermissionAction::Default;
                if (isAudio && isVideo) {
                    // Both — check microphone first, then camera
                    auto micAction = getConfiguredPermission(*cfg, "microphone");
                    auto camAction = getConfiguredPermission(*cfg, "camera");
                    if (micAction == PermissionAction::Allow && camAction == PermissionAction::Allow) {
                        action = PermissionAction::Allow;
                    } else if (micAction == PermissionAction::Deny || camAction == PermissionAction::Deny) {
                        action = PermissionAction::Deny;
                    }
                } else if (isAudio) {
                    action = getConfiguredPermission(*cfg, "microphone");
                } else if (isVideo) {
                    action = getConfiguredPermission(*cfg, "camera");
                }

                if (action == PermissionAction::Allow) {
                    webkit_permission_request_allow(request);
                } else if (action == PermissionAction::Deny) {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // Website data access (storage access)
            if (WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)) {
                auto action = getConfiguredPermission(*cfg, "storageAccess");
                if (action == PermissionAction::Allow || autoAllow) {
                    webkit_permission_request_allow(request);
                } else if (action == PermissionAction::Deny) {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // Media key system (DRM / encrypted media)
            if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)) {
                if (autoAllow) {
                    webkit_permission_request_allow(request);
                    return TRUE;
                }
                // No direct config mapping; default to allow if autoAllowAll
                webkit_permission_request_allow(request);
                return TRUE;
            }

            // Device info
            if (G_TYPE_CHECK_INSTANCE_TYPE(request,
                    webkit_device_info_permission_request_get_type())) {
                if (autoAllow) {
                    webkit_permission_request_allow(request);
                } else {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // Install missing media plugins
            if (WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)) {
                if (autoAllow) {
                    webkit_permission_request_allow(request);
                } else {
                    webkit_permission_request_deny(request);
                }
                return TRUE;
            }

            // Default: auto-allow if configured, otherwise let WebKit decide
            if (autoAllow) {
                webkit_permission_request_allow(request);
                return TRUE;
            }

            return FALSE; // Let default handler process
        }),
        configPtr,
        +[](gpointer data, GClosure*) {
            delete static_cast<nlohmann::json*>(data);
        },
        (GConnectFlags)0
    );
}

// ============================================================================
//  Features Configuration
// ============================================================================

static void applyFeaturesConfig(WebKitWebView* webview, const nlohmann::json& config) {
    auto features = config.value("features", nlohmann::json::object());
    WebKitSettings* settings = webkit_web_view_get_settings(webview);

    // experimentalWebPlatformFeatures: enable various experimental settings
    if (features.value("experimentalWebPlatformFeatures", false)) {
        webkit_settings_set_enable_webgl(settings, TRUE);
        webkit_settings_set_enable_webaudio(settings, TRUE);
        webkit_settings_set_enable_mediasource(settings, TRUE);
        webkit_settings_set_enable_media_stream(settings, TRUE);
        webkit_settings_set_enable_media_capabilities(settings, TRUE);
        webkit_settings_set_enable_encrypted_media(settings, TRUE);
        webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
        webkit_settings_set_enable_back_forward_navigation_gestures(settings, TRUE);
        webkit_settings_set_enable_dns_prefetching(settings, TRUE);
        webkit_settings_set_enable_itp(settings, FALSE);
    }

    // webGPU: enable if available (WebKitGTK 2.42+)
#if WEBKIT_CHECK_VERSION(2, 42, 0)
    if (features.value("webGPU", false)) {
        webkit_settings_set_enable_webgpu(settings, TRUE);
    }
#endif

    // fullscreen
    if (features.value("fullscreen", false)) {
        webkit_settings_set_enable_fullscreen(settings, TRUE);
    }

    // clipboardWrite: no direct setting in WebKitGTK, enabled by default

    // virtualReality / augmentedReality: WebXR support
    // No direct WebKitSettings toggle; enabled by default in recent versions

    // sensors: no direct WebKitSettings toggle

    // wakeLock: no direct WebKitSettings toggle

    // pictureInPicture: no direct WebKitSettings toggle in older versions
    // Enabled by default in WebKitGTK 2.36+

    // webAuthn: supported by default in recent WebKitGTK

    // webBluetooth, webUSB, webSerial, webHID, webNFC, webShare,
    // contacts, handwriting, computePressure, smartCard, webTransport,
    // federatedCredentials, digitalGoods, paymentRequest:
    // No direct WebKitSettings toggles; support depends on WebKitGTK version.
    // These are either enabled by default or not supported.

    // PNA is handled in applySecurityConfig via setupPnaConfiguration

    // NOTE: Many feature flags are Chromium-specific and have no WebKitGTK
    // equivalent. WebKitGTK enables supported features by default.
}

// ============================================================================
//  Settings Configuration
// ============================================================================

void applyConfigToWebkitSettings(WebKitWebView* webview, const nlohmann::json& config) {
    WebKitSettings* settings = webkit_web_view_get_settings(webview);
    auto security = config.value("security", nlohmann::json::object());
    auto permissions = config.value("permissions", nlohmann::json::object());

    // Autoplay
    bool autoplayAllowed = (getConfiguredPermission(config, "autoplay") == PermissionAction::Allow)
                        || permissions.value("autoAllowAll", false);
    webkit_settings_set_media_playback_requires_user_gesture(settings,
        autoplayAllowed ? FALSE : TRUE);

    // File access
    webkit_settings_set_allow_file_access_from_file_urls(settings,
        security.value("allowFileAccessFromFiles", false) ? TRUE : FALSE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings,
        security.value("allowFileAccess", false) ? TRUE : FALSE);

    // Enable JavaScript
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(settings, TRUE);

    // Enable page cache, local storage, databases
    webkit_settings_set_enable_page_cache(settings, TRUE);
    webkit_settings_set_enable_html5_local_storage(settings, TRUE);
    webkit_settings_set_enable_html5_database(settings, TRUE);
    webkit_settings_set_enable_offline_web_application_cache(settings, TRUE);

    // Enable WebGL, WebAudio, media
    webkit_settings_set_enable_webgl(settings, TRUE);
    webkit_settings_set_enable_webaudio(settings, TRUE);
    webkit_settings_set_enable_media(settings, TRUE);
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_mediasource(settings, TRUE);

    // Enable spatial navigation, tabs to links
    webkit_settings_set_enable_spatial_navigation(settings, TRUE);
    webkit_settings_set_enable_tabs_to_links(settings, TRUE);

    // Disable ITP (Intelligent Tracking Prevention) for development
    if (config.value("env", "development") == "development") {
        webkit_settings_set_enable_itp(settings, FALSE);
    }

    // Developer extras
    if (config.value("env", "development") != "development") return;
    webkit_settings_set_enable_developer_extras(settings, TRUE);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webview);
    webkit_web_inspector_show(inspector);
}

// ============================================================================
//  Main Application Functions
// ============================================================================

void runApplication(int argc, char **argv) {
    auto& config = getConfig();
    auto& app = Globals::app;

    app = gtk_application_new(
        config.value("id", "org.webnative.example").c_str(),
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    Globals::exitStatus = g_application_run(G_APPLICATION(app), argc, argv);
}

void activate(GtkApplication *app) {
    auto& config = getConfig();

    // Configure web context BEFORE creating webview
    setupWebContext(config);

    Globals::webview = createWebviewWindow(app, config);

    // Register message handler for JS→C++ bridge
    webkit_user_content_manager_register_script_message_handler(
        Globals::contentManager, "webnative", NULL
    );

    g_signal_connect(
        Globals::contentManager,
        "script-message-received::webnative",
        G_CALLBACK(onApiCall),
        NULL
    );

    // Inject postMessage bridge script
    std::string bridgeScript = R"(
        window.sendSignalToCpp = function(msg) {
            window.webkit.messageHandlers.webnative.postMessage(msg);
        };
    )";
    injectUserScript(Globals::contentManager, bridgeScript);

    // Apply security, permissions, and features configuration
    applySecurityConfig(Globals::webview, config);
    setupPermissionHandler(Globals::webview, config);
    applyFeaturesConfig(Globals::webview, config);
}

WebKitWebView* createWebviewWindow(GtkApplication *app, const nlohmann::json& config) {
    auto window = gtk_application_window_new(app);
    applyConfigToWindow(window, config);

    auto webview = createWebview();
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(webview));
    loadHtmlToWebview(webview, "index.html");

    applyConfigToWebkitSettings(webview, config);
    gtk_window_present(GTK_WINDOW(window));
    return webview;
}

WebKitWebView* createWebview() {
    Globals::contentManager = webkit_user_content_manager_new();

    WebKitWebView* webview = WEBKIT_WEB_VIEW(
        g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "user-content-manager", Globals::contentManager,
            NULL)
    );

    return webview;
}

void applyConfigToWindow(GtkWidget* window, const nlohmann::json& config) {
    gtk_window_set_title(GTK_WINDOW(window),
        config.value("name", "Webnative application").c_str());

    auto windowConfig = config.value("window", nlohmann::json::object());
    if (windowConfig.value("fullscreen", false))
        gtk_window_fullscreen(GTK_WINDOW(window));

    gtk_widget_set_size_request(
        window,
        windowConfig.value("minWidth", 500),
        windowConfig.value("minHeight", 700)
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        windowConfig.value("width", 1200),
        windowConfig.value("height", 700)
    );
}

void loadHtmlToWebview(WebKitWebView* webview, const std::string& html) {
    std::string publicPath = Globals::config.value("appDir", ".") + "/usr/bin/public/" + html;
    GFile* file = g_file_new_for_path(publicPath.c_str());
    gchar* baseURL = g_file_get_uri(file);
    g_object_unref(file);
    webkit_web_view_load_uri(webview, baseURL);
    g_free(baseURL);
}

void onApiCall(WebKitUserContentManager *manager, JSCValue *value) {
    gchar* gstring = jsc_value_to_string(value);
    std::string data(gstring);
    g_free(gstring);
    handleApiCall(manager, data);
}

void terminateWebkitApp() {
    g_object_unref(Globals::app);
    std::exit(Globals::exitStatus);
}