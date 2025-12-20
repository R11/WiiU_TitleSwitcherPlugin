#!/bin/bash
# Hot reload development server for web preview
# Watches for file changes, rebuilds, and auto-refreshes browser via WebSocket

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SRC_DIR="$SCRIPT_DIR/../../src"
SERVER_URL="http://localhost:8080"

# Check dependencies
check_deps() {
    local missing=()
    command -v fswatch >/dev/null 2>&1 || missing+=("fswatch (brew install fswatch)")
    command -v node >/dev/null 2>&1 || missing+=("node (install Node.js)")

    if [ ${#missing[@]} -gt 0 ]; then
        echo "Missing dependencies:"
        for dep in "${missing[@]}"; do
            echo "  - $dep"
        done
        exit 1
    fi
}

# Build once before starting
initial_build() {
    echo "Building web preview..."
    cd "$BUILD_DIR"
    emmake make 2>&1 | tail -3
    echo "Build complete."
}

# Notify server of build status
notify() {
    curl -s -X POST "$SERVER_URL/$1" -d "$2" >/dev/null 2>&1 || true
}

# Watch and rebuild on changes
watch_and_build() {
    # Wait for server to start
    sleep 1
    echo "Watching for changes..."

    fswatch -o \
        "$SRC_DIR" \
        "$SCRIPT_DIR"/*.cpp \
        "$SCRIPT_DIR"/*.html \
        "$SCRIPT_DIR"/stubs \
        2>/dev/null | while read -r; do
        echo ""
        echo "Change detected, rebuilding..."
        notify "building"

        cd "$BUILD_DIR"
        # Force relink for shell.html changes
        rm -f preview.html 2>/dev/null || true

        if emmake make 2>&1 | tail -5; then
            echo "Rebuild complete."
            notify "reload"
        else
            echo "Build failed!"
            notify "error" "Check terminal for details"
        fi
    done
}

# Start WebSocket dev server
start_server() {
    cd "$SCRIPT_DIR"
    node dev-server.js
}

# Cleanup on exit
cleanup() {
    echo ""
    echo "Shutting down..."
    kill $(jobs -p) 2>/dev/null
    exit 0
}

trap cleanup SIGINT SIGTERM

# Main
check_deps
initial_build

echo ""

# Run server first, then watcher
start_server &
watch_and_build &

wait
