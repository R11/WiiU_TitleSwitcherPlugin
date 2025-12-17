#!/bin/bash
set -euo pipefail

# Only run on Claude Code remote environment
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

echo "=== Setting up WiiU_TitleSwitcherPlugin environment ==="

# Install build dependencies if not present
if ! command -v g++ &> /dev/null || ! dpkg -s libgtest-dev &> /dev/null 2>&1; then
    echo "Installing build dependencies..."
    apt-get update
    apt-get install -y g++ make libgtest-dev cmake
fi

# Build Google Test libraries if not present (Ubuntu packages source only)
if [ ! -f /usr/lib/libgtest.a ]; then
    echo "Building Google Test libraries..."
    cd /usr/src/gtest
    cmake .
    make
    cp lib/*.a /usr/lib/
fi

cd "$CLAUDE_PROJECT_DIR"

# Install git hooks
echo "Installing git hooks..."
./scripts/install-hooks.sh

# Build unit tests
echo "Building unit tests..."
cd "$CLAUDE_PROJECT_DIR/tests"
make

# Build preview tool
echo "Building preview tool..."
cd "$CLAUDE_PROJECT_DIR/tools/preview"
make

echo "=== Environment setup complete ==="
