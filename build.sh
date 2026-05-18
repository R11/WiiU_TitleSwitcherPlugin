#!/bin/bash
# Build script for TitleSwitcherPlugin
# Builds the plugin using Docker and extracts the .wps file
#
# Usage:
#   ./build.sh              # Build only
#   ./build.sh --package    # Build and create SD card package
#   ./build.sh --release    # Build, package, and create zip

set -e

# Parse arguments
DO_PACKAGE=false
DO_ZIP=false

for arg in "$@"; do
    case $arg in
        --package)
            DO_PACKAGE=true
            ;;
        --release)
            DO_PACKAGE=true
            DO_ZIP=true
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --package  Build and create SD card folder structure in dist/"
            echo "  --release  Build, package, and create zip archive"
            echo "  --help     Show this help message"
            exit 0
            ;;
    esac
done

echo "Building TitleSwitcher (plugin + app)..."
docker build -t titleswitcherplugin .

echo "Extracting build artifacts..."
docker run --rm -v "$(pwd):/output" titleswitcherplugin \
    sh -c "cp /project/TitleSwitcherPlugin.wps /output/ && cp /project/TitleSwitcher.wuhb /output/"

echo "Build complete!"
ls -la TitleSwitcherPlugin.wps TitleSwitcher.wuhb

# Run packaging if requested
if [ "$DO_PACKAGE" = true ]; then
    echo ""
    if [ "$DO_ZIP" = true ]; then
        ./scripts/package.sh --zip
    else
        ./scripts/package.sh
    fi
fi
