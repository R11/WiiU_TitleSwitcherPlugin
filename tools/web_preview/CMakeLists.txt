cmake_minimum_required(VERSION 3.13)
project(WiiUMenuPreview)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Linker flags (these are link-time options)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s WASM=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_FUNCTIONS=[_main,_onKeyDown,_onKeyUp,_handleKeyDown,_handleKeyUp,_setTvResolution,_selectScreen]")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s EXPORTED_RUNTIME_METHODS=[ccall,cwrap,HEAPU8]")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_SDL=0")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
endif()

# Define we're building for web preview
add_definitions(-DWEB_PREVIEW)
add_definitions(-D__WIIU__)  # For compatibility with some headers

# Include directories
# web_preview/stubs first (shadows WUT system headers like <sysapp/launch.h>)
# then preview/stubs for shared stubs
# then main project src
include_directories(
    ${CMAKE_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/../preview/stubs
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../../src
)

# Source files from main project
# Using the actual menu.cpp gives us the full state machine
set(MENU_SOURCES
    ${CMAKE_SOURCE_DIR}/../../src/menu/menu.cpp
    ${CMAKE_SOURCE_DIR}/../../src/menu/categories.cpp
    ${CMAKE_SOURCE_DIR}/../../src/ui/list_view.cpp
    ${CMAKE_SOURCE_DIR}/../../src/input/text_input.cpp
    ${CMAKE_SOURCE_DIR}/../../src/render/measurements.cpp
)

# Web preview specific sources
# Mock implementations provide stubs for Settings, Titles, ImageLoader, TitlePresets
set(PREVIEW_SOURCES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/canvas_renderer.cpp
    ${CMAKE_SOURCE_DIR}/web_input.cpp
    ${CMAKE_SOURCE_DIR}/mock_settings.cpp
    ${CMAKE_SOURCE_DIR}/mock_titles.cpp
    ${CMAKE_SOURCE_DIR}/mock_image_loader.cpp
    ${CMAKE_SOURCE_DIR}/mock_title_presets.cpp
)

add_executable(preview ${MENU_SOURCES} ${PREVIEW_SOURCES})

# Note: shell.html is used as template via --shell-file flag
# The output will be preview.html in the build directory
