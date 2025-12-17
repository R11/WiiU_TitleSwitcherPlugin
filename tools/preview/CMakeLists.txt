# Preview Tool CMakeLists.txt for TitleSwitcherPlugin
# Builds ASCII preview tool for visualizing menu layouts

cmake_minimum_required(VERSION 3.14)
project(TitleSwitcherPreview CXX)

# Preview executable
add_executable(preview_demo
    preview_demo.cpp
    menu_render.cpp
)

target_include_directories(preview_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(preview_demo PRIVATE cxx_std_17)
target_compile_options(preview_demo PRIVATE -Wall -Wextra -O2)

# Custom targets for convenience
add_custom_target(run
    COMMAND preview_demo
    DEPENDS preview_demo
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(update-snapshots
    COMMAND preview_demo --update-snapshots
    DEPENDS preview_demo
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(verify-snapshots
    COMMAND preview_demo --verify-snapshots
    DEPENDS preview_demo
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
