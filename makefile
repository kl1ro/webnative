compiler = g++
compilerFlags = -std=c++11 -Wall `pkg-config --cflags gtk+-3.0 webkit2gtk-4.0`
linkerFlags = `pkg-config --libs gtk+-3.0 webkit2gtk-4.0`
builderTarget = appimagetool-x86_64.AppImage
coreSource = core/linux.cpp
coreTarget = bin/usr/bin/linux64
backendTarget = bin/usr/bin/backend
backendSource = app/backend
publicSource = app/public
publicTarget = bin/usr/bin/public
distTarget = dist/linux64
appimagetoolTarget = core/tools/appimagetool
toolsFolder = core/tools

# Default coreTarget
all: $(builderTarget) $(coreTarget) $(backendTarget) $(publicTarget) $(distTarget)

$(builderTarget):
	mkdir ${toolsFolder} -p
	if [ ! -f ${appimagetoolTarget} ]; then \
		wget -O ${appimagetoolTarget} https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage; \
		chmod +x ${appimagetoolTarget}; \
	fi

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