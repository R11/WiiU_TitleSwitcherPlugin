# Test CMakeLists.txt for TitleSwitcherPlugin
# Builds unit tests using vendored Google Test (portable across all platforms)

cmake_minimum_required(VERSION 3.14)
project(TitleSwitcherTests CXX)

# Use vendored googletest from submodule
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../extern/googletest googletest EXCLUDE_FROM_ALL)

# Test executable
add_executable(run_tests
    unit/buttons_test.cpp
    unit/settings_test.cpp
    unit/list_view_test.cpp
    ../src/storage/settings.cpp
    ../src/ui/list_view.cpp
)

target_include_directories(run_tests PRIVATE
    mocks
    ../src
)

target_compile_features(run_tests PRIVATE cxx_std_17)
target_compile_definitions(run_tests PRIVATE TEST_BUILD)
target_compile_options(run_tests PRIVATE -Wall -Wextra)

target_link_libraries(run_tests
    gtest
    gtest_main
)

# Enable testing
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)
