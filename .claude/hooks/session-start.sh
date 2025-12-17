#!/bin/bash
set -euo pipefail

# Only run on Claude Code remote environment
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

# Run asynchronously - session starts immediately while this runs in background
echo '{"async": true, "asyncTimeout": 300000}'

echo "=== Setting up WiiU_TitleSwitcherPlugin environment ==="

cd "$CLAUDE_PROJECT_DIR"

# Install minimal build dependencies (cmake, g++, make - no gtest needed!)
if ! command -v cmake &> /dev/null || ! command -v g++ &> /dev/null; then
    echo "Installing build dependencies..."
    apt-get update
    apt-get install -y g++ make cmake
fi

# Initialize git submodules (vendored googletest)
if [ ! -f extern/googletest/CMakeLists.txt ]; then
    echo "Initializing git submodules..."
    git submodule update --init --recursive
fi

# Install git hooks
echo "Installing git hooks..."
./scripts/install-hooks.sh

# Build unit tests with CMake (uses vendored googletest)
echo "Building unit tests..."
cmake -S tests -B tests/build
cmake --build tests/build

# Copy executable to expected location for compatibility
cp tests/build/run_tests tests/run_tests

# Build preview tool with CMake
echo "Building preview tool..."
cmake -S tools/preview -B tools/preview/build
cmake --build tools/preview/build

# Copy executable to expected location for compatibility
cp tools/preview/build/preview_demo tools/preview/preview_demo

echo "=== Environment setup complete ==="
