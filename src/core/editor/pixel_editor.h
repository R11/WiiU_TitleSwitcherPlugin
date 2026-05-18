/**
 * Pixel Editor
 *
 * A simple pixel art editor using libgd for canvas operations.
 * Inspired by draw.js canvas implementation.
 *
 * FEATURES:
 * ---------
 * - Multiple zoom levels (1x, 2x, 4x, 8x)
 * - Grid overlay matching zoom
 * - Color palette selection
 * - Tools: Pencil, Eraser, Fill
 * - Minimap preview
 * - Save/Load PNG to SD card
 *
 * CONTROLS:
 * ---------
 * - D-Pad: Move cursor
 * - A: Draw pixel / Select
 * - B: Cancel / Back
 * - X: Eraser toggle
 * - Y: Fill tool
 * - L/R: Zoom out/in
 * - +/-: Grid size
 * - Touch: Direct drawing
 *
 * USAGE:
 * ------
 *   PixelEditor::Config config;
 *   config.width = 64;
 *   config.height = 64;
 *   // savePath defaults to Paths::USER_DATA_DIR (sd:/wiiu/titleswitcher/)
 *
 *   PixelEditor::Open(config);  // Blocks until closed
 */

#pragma once

#include <cstdint>
#include "../utils/paths.h"

namespace PixelEditor {

// =============================================================================
// Configuration
// =============================================================================

struct Config {
    int width = 64;              // Canvas width in pixels
    int height = 64;             // Canvas height in pixels
    const char* savePath = Paths::USER_DATA_DIR;  // Defined in utils/paths.h
    const char* loadFile = nullptr;  // Optional: load existing file
};

// =============================================================================
// Tool Types
// =============================================================================

enum class Tool {
    PENCIL,
    ERASER,
    FILL,
    COLOR_PICKER
};

// =============================================================================
// Color Palette
// =============================================================================

// 16-color palette (expandable)
constexpr int PALETTE_SIZE = 16;

struct Palette {
    uint32_t colors[PALETTE_SIZE];
    int count;
    const char* name;
};

// Built-in palettes
extern const Palette PALETTE_DEFAULT;
extern const Palette PALETTE_GAMEBOY;
extern const Palette PALETTE_NES;

// =============================================================================
// Lifecycle
// =============================================================================

/**
 * Open the pixel editor.
 * Blocks until the user closes it.
 *
 * @param config Editor configuration
 * @return true if image was saved, false if cancelled
 */
bool Open(const Config& config);

/**
 * Open editor to edit an existing image.
 *
 * @param filePath Path to PNG/TGA file
 * @return true if saved, false if cancelled
 */
bool Edit(const char* filePath);

// =============================================================================
// State (for external access if needed)
// =============================================================================

/**
 * Get the current canvas as RGBA pixel data.
 * Only valid while editor is open.
 */
uint32_t* GetPixels();
int GetWidth();
int GetHeight();

} // namespace PixelEditor
