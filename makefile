compiler = g++
compilerFlags = -std=c++11 -Wall `pkg-config --cflags gtk+-3.0 webkit2gtk-4.0`
linkerFlags = `pkg-config --libs gtk+-3.0 webkit2gtk-4.0`

coreSource = core/linux.cpp
coreTarget = bin/usr/bin/linux64

backendTarget = bin/usr/bin/backend
backendSource = app/backend

publicSource = app/public
publicTarget = bin/usr/bin/public

linuxTarget = dist/linux64
windowsTarget = dist/windows64

appimagetoolTarget = core/tools/appimagetool
webview2Target = core/tools/webview2
toolsFolder = core/tools

# Default coreTarget
all: $(appimagetoolTarget) $(webview2) $(coreTarget) $(backendTarget) $(publicTarget) $(distTarget)

$(appimagetoolTarget):
	mkdir ${toolsFolder} -p
	if [ ! -f ${appimagetoolTarget} ]; then \
		wget -O ${appimagetoolTarget} https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage; \
		chmod +x ${appimagetoolTarget}; \
	fi

$(webview2Target):
	nuget install Microsoft.Web.WebView2 -Version 1.0.1370.28 -OutputDirectory $(toolsFolder)
	mv $(toolsFolder)/Microsoft.Web.WebView2.1.0.1370.28 $(webview2Target)

# Compile the coreSource
$(coreTarget): $(coreSource)
	mkdir bin/usr/bin -p
	cp core/app-stuff/AppRun bin
	cp core/app-stuff/myapp.desktop bin
	cp core/app-stuff/icon.png bin
	$(compiler) $(coreSource) $(compilerFlags) -o $(coreTarget) $(linkerFlags)

$(backendTarget): $(backendSource)
	cp $(backendSource) $(backendTarget) -r

# Copy the compiled public folder
$(publicTarget): $(publicSource)
	cp $(publicSource) $(publicTarget) -r

# Make a linux64 dist
$(distTarget): $(builderTarget) $(coreTarget) $(backendTarget) $(backendTarget)
	mkdir dist -p
	rm dist/linux64 -rf
	./${appimagetoolTarget} bin/ -n dist/linux64
	rm bin -rf

clean:
	rm bin dist -rf