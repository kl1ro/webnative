cmake_minimum_required(VERSION 3.15)
project(webnative)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk4)
pkg_check_modules(WEBKIT REQUIRED webkitgtk-6.0)

include(FetchContent)

FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(json)

add_executable(
	linux 
	src/api.cpp
	src/config.cpp
	src/globals.cpp
	src/ipc.cpp
	src/linux.cpp
	src/node.cpp
	src/webkit.cpp
)

target_include_directories(linux PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${stb_SOURCE_DIR}
)

target_include_directories(linux PRIVATE
	${GTK_INCLUDE_DIRS}
	${WEBKIT_INCLUDE_DIRS}
)

target_link_libraries(linux PRIVATE
	${GTK_LIBRARIES}
	${WEBKIT_LIBRARIES}
	nlohmann_json::nlohmann_json
)

target_compile_options(linux PRIVATE
	${GTK_CFLAGS_OTHER}
	${WEBKIT_CFLAGS_OTHER}
)