#!/bin/bash
# Package script for TitleSwitcherPlugin
#
# Creates an SD card-ready folder structure and optional zip file
# that can be directly extracted to a Wii U SD card.
#
# Usage:
#   ./scripts/package.sh           # Package only (requires existing .wps)
#   ./scripts/package.sh --build   # Build first, then package
#   ./scripts/package.sh --zip     # Create zip archive
#   ./scripts/package.sh --all     # Build, package, and create zip

set -e

# =============================================================================
# Configuration - Edit these paths to match paths.h if needed
# =============================================================================

# Plugin binary name
PLUGIN_NAME="TitleSwitcherPlugin"
PLUGIN_EXT="wps"

# Output directory (SD card structure will be created here)
DIST_DIR="dist"

# SD card paths (relative to SD root)
# These should match the paths in src/utils/paths.h
PLUGIN_INSTALL_PATH="wiiu/environments/aroma/plugins"
PRESETS_CONFIG_PATH="wiiu/environments/aroma/plugins/config"
USER_DATA_PATH="wiiu/titleswitcher"

# Source files
PRESETS_SOURCE="examples/TitleSwitcher_presets.json"

# =============================================================================
# Parse arguments
# =============================================================================

DO_BUILD=false
DO_ZIP=false

for arg in "$@"; do
    case $arg in
        --build)
            DO_BUILD=true
            ;;
        --zip)
            DO_ZIP=true
            ;;
        --all)
            DO_BUILD=true
            DO_ZIP=true
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build    Build the plugin before packaging"
            echo "  --zip      Create a zip archive after packaging"
            echo "  --all      Build, package, and create zip"
            echo "  --help     Show this help message"
            echo ""
            echo "Output structure in $DIST_DIR/:"
            echo "  wiiu/"
            echo "  └── environments/"
            echo "      └── aroma/"
            echo "          └── plugins/"
            echo "              ├── ${PLUGIN_NAME}.${PLUGIN_EXT}"
            echo "              └── config/"
            echo "                  └── TitleSwitcher_presets.json"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# =============================================================================
# Build if requested
# =============================================================================

if [ "$DO_BUILD" = true ]; then
    echo "=== Building plugin ==="
    ./build.sh
    echo ""
fi

# =============================================================================
# Verify plugin exists
# =============================================================================

PLUGIN_FILE="${PLUGIN_NAME}.${PLUGIN_EXT}"

if [ ! -f "$PLUGIN_FILE" ]; then
    echo "Error: $PLUGIN_FILE not found!"
    echo "Run with --build to build the plugin first, or run ./build.sh"
    exit 1
fi

echo "=== Packaging $PLUGIN_NAME ==="

# =============================================================================
# Create directory structure
# =============================================================================

echo "Creating SD card directory structure..."

# Clean previous dist
rm -rf "$DIST_DIR"

# Create directories
mkdir -p "$DIST_DIR/$PLUGIN_INSTALL_PATH"
mkdir -p "$DIST_DIR/$PRESETS_CONFIG_PATH"
mkdir -p "$DIST_DIR/$USER_DATA_PATH"

# =============================================================================
# Copy files
# =============================================================================

echo "Copying files..."

# Copy plugin binary
cp "$PLUGIN_FILE" "$DIST_DIR/$PLUGIN_INSTALL_PATH/"
echo "  ✓ $PLUGIN_FILE -> $PLUGIN_INSTALL_PATH/"

# Copy presets file if it exists
if [ -f "$PRESETS_SOURCE" ]; then
    cp "$PRESETS_SOURCE" "$DIST_DIR/$PRESETS_CONFIG_PATH/"
    echo "  ✓ TitleSwitcher_presets.json -> $PRESETS_CONFIG_PATH/"
else
    echo "  ⚠ Warning: $PRESETS_SOURCE not found (optional)"
fi

# Create a placeholder README in user data directory
cat > "$DIST_DIR/$USER_DATA_PATH/README.txt" << 'EOF'
Title Switcher User Data Directory
===================================

This directory is used by Title Switcher to store user-created content
such as pixel art drawings.

You can safely delete this directory if you don't use these features.
EOF
echo "  ✓ Created README.txt in $USER_DATA_PATH/"

# =============================================================================
# Create installation guide
# =============================================================================

cat > "$DIST_DIR/INSTALL.txt" << 'EOF'
Title Switcher Plugin - Installation Guide
==========================================

QUICK INSTALL:
--------------
1. Copy the entire 'wiiu' folder to the root of your SD card
2. Boot your Wii U with Aroma
3. Press L + D-Pad Down + Minus in any app to open Title Switcher

MANUAL INSTALL:
---------------
Copy files to your SD card:

  TitleSwitcherPlugin.wps
    -> sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps

  TitleSwitcher_presets.json (optional, for game metadata)
    -> sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher_presets.json

CONTROLS:
---------
  L + D-Pad Down + Minus : Open Title Switcher menu
  A / + : Launch selected game
  B : Close menu
  D-Pad Up/Down : Navigate
  ZL/ZR : Switch categories
  Y : Toggle favorite
  X : Open settings

NOTES:
------
- Settings are automatically saved to:
  sd:/wiiu/plugins/config/TitleSwitcher.json

- The presets file provides game metadata (publisher, developer, etc.)
  It's optional but recommended for the best experience.

For more information, visit:
https://github.com/R11/WiiU_TitleSwitcherPlugin
EOF

echo "  ✓ Created INSTALL.txt"

# =============================================================================
# Show summary
# =============================================================================

echo ""
echo "=== Package contents ==="
find "$DIST_DIR" -type f | sort | while read -r file; do
    size=$(du -h "$file" | cut -f1)
    echo "  $size  ${file#$DIST_DIR/}"
done

# =============================================================================
# Create zip if requested
# =============================================================================

if [ "$DO_ZIP" = true ]; then
    echo ""
    echo "=== Creating zip archive ==="

    ZIP_NAME="${PLUGIN_NAME}.zip"

    # Create zip from dist directory contents
    cd "$DIST_DIR"
    zip -r "../$ZIP_NAME" . -x "*.DS_Store"
    cd ..

    ZIP_SIZE=$(du -h "$ZIP_NAME" | cut -f1)
    echo "  ✓ Created $ZIP_NAME ($ZIP_SIZE)"
    echo ""
    echo "To install: Extract $ZIP_NAME to the root of your SD card"
fi

echo ""
echo "=== Packaging complete ==="
echo "Output directory: $DIST_DIR/"
