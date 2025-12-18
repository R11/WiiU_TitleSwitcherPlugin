cmake_minimum_required(VERSION 3.13)
project(WiiUMenuPreview)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")

    # Emscripten flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORTED_FUNCTIONS='[\"_main\",\"_onKeyDown\",\"_onKeyUp\",\"_setTvResolution\",\"_selectScreen\"]'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\"]'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/shell.html")
endif()

# Define we're building for web preview
add_definitions(-DWEB_PREVIEW)
add_definitions(-D__WIIU__)  # For compatibility with some headers

# Include directories - stubs first to override main project headers
include_directories(
    ${CMAKE_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../../src
)

# Source files - actual menu logic from main project
set(MENU_SOURCES
    ${CMAKE_SOURCE_DIR}/../../src/menu/categories.cpp
    ${CMAKE_SOURCE_DIR}/../../src/ui/list_view.cpp
)

# Web preview specific sources
set(PREVIEW_SOURCES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/canvas_renderer.cpp
    ${CMAKE_SOURCE_DIR}/web_input.cpp
    ${CMAKE_SOURCE_DIR}/mock_settings.cpp
    ${CMAKE_SOURCE_DIR}/mock_titles.cpp
)

add_executable(preview ${MENU_SOURCES} ${PREVIEW_SOURCES})

# Note: shell.html is used as template via --shell-file flag
# The output will be preview.html in the build directory
