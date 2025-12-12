#!/bin/bash
# Build script for TitleSwitcherPlugin
# Builds the plugin using Docker and extracts the .wps file

set -e

echo "Building TitleSwitcherPlugin..."
docker build -t titleswitcherplugin .

echo "Extracting TitleSwitcherPlugin.wps..."
docker run --rm -v "$(pwd):/output" titleswitcherplugin cp /project/TitleSwitcherPlugin.wps /output/

echo "Build complete!"
ls -la TitleSwitcherPlugin.wps
