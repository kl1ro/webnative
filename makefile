compiler = g++
compilerFlags = -std=c++11 -Wall `pkg-config --cflags gtk+-3.0 webkit2gtk-4.0`
linkerFlags = `pkg-config --libs gtk+-3.0 webkit2gtk-4.0`
builderTarget = appimagetool-x86_64.AppImage
coreSource = core/renderer.cpp
coreTarget = bin/usr/bin/linux64
backendTarget = bin/usr/bin/backend.js
backendSource = app/backend.js
publicSource = app/public
publicTarget = bin/usr/bin/public
distTarget = dist/linux64

# Default coreTarget
all: $(builderTarget) $(coreTarget) $(backendTarget) $(publicTarget) $(distTarget)

$(builderTarget):
	wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x appimagetool-x86_64.AppImage

# Compile the coreSource
$(coreTarget): $(coreSource)
	$(compiler) $(coreSource) $(compilerFlags) -o $(coreTarget) $(linkerFlags)

$(backendTarget): $(backendSource)
	cp $(backendSource) $(backendTarget)

# Copy the compiled public folder
$(publicTarget): $(publicSource)
	cp $(publicSource) $(publicTarget) -r

# Make a linux64 dist
$(distTarget): $(builderTarget) $(coreTarget) $(backendTarget) $(backendTarget)
	rm dist -r
	mkdir dist
	./appimagetool-x86_64.AppImage bin/ -n dist/linux64

clean:
	rm -rf bin/usr/bin/*
