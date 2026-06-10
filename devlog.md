# Project goal
- Any platform can build for any target with almost zero friction for developer
- The application itself is always web based
- Frontend is executed on native WebView component to minimize bundle size
- Any backend can be used on desktop platforms, as long as it implements the webnative IPC interface
- Mobile platforms can't support backend, so api abstraction layer is necessary.
- The cli tool also should be able to create self-contained apps, but in this case the bundle size don't matter
- Always try to build natively. If not possible, fall back to Docker. If Docker is not installed, xplain clearly why Docker is needed and how to install it manually.

## ⏳ means that the feature is under development

## v1 (status: implemented)
- ✅ Linux target only
- ✅ Self-contained AppImage build pipeline
- ✅ Optional Node.js backend support
- ✅ Dev environment: Linux only
- No Windows/macOS/mobile *targets* in v1

## v2 (status: implemented)
- ✅ Windows and Linux self-contained targets
- ✅ Dev environment: Linux only
- No macOS/mobile *targets* in v2

## v3 (status: implemented)
- ✅ Windows and Linux self-contained targets
- ✅ Android apk target supported
- ✅ Dev environment: Linux only
- No macOS/mobile *targets* in v3
