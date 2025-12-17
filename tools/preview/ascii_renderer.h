/**
 * ASCII Screen Preview Renderer
 *
 * A desktop-compatible renderer that outputs to terminal using ASCII/Unicode
 * characters. Useful for previewing Wii U menu layouts without hardware.
 *
 * Supports two display modes:
 * - DRC (GamePad): 854x480 pixels, 100 cols x 18 rows text grid
 * - TV: 1920x1080 pixels (scaled down for terminal display)
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AsciiRenderer {

// Screen types
enum class Screen {
    DRC,    // GamePad (854x480, 100x18 grid)
    TV      // TV (1920x1080, 240x45 grid)
};

// Screen dimensions
struct ScreenConfig {
    int pixelWidth;
    int pixelHeight;
    int gridCols;
    int gridRows;
    int charWidth;   // Pixels per character
    int charHeight;  // Pixels per character
};

// Get configuration for a screen type
const ScreenConfig& GetScreenConfig(Screen screen);

// Image data (simplified version of Renderer::ImageData)
struct ImageData {
    std::vector<uint32_t> pixels;  // RGBA pixels (0xRRGGBBAA)
    int width;
    int height;
};

using ImageHandle = const ImageData*;
constexpr ImageHandle INVALID_IMAGE = nullptr;

/**
 * Initialize the ASCII renderer for a specific screen.
 */
void Init(Screen screen);

/**
 * Shut down the renderer.
 */
void Shutdown();

/**
 * Begin a new frame with the specified background color.
 */
void BeginFrame(uint32_t clearColor);

/**
 * End the frame and output to terminal.
 *
 * @param useColor  Use ANSI color codes (true) or plain ASCII (false)
 * @param useUnicode Use Unicode block characters for better fidelity
 */
void EndFrame(bool useColor = true, bool useUnicode = true);

/**
 * Draw text at grid position.
 */
void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF);

/**
 * Draw formatted text.
 */
void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...);

/**
 * Draw an image at pixel coordinates.
 */
void DrawImage(int x, int y, ImageHandle image, int width = 0, int height = 0);

/**
 * Draw a placeholder rectangle.
 */
void DrawPlaceholder(int x, int y, int width, int height, uint32_t color);

/**
 * Draw a horizontal line using box-drawing characters.
 */
void DrawHLine(int col, int row, int length, uint32_t color = 0xFFFFFFFF);

/**
 * Draw a vertical line using box-drawing characters.
 */
void DrawVLine(int col, int row, int length, uint32_t color = 0xFFFFFFFF);

/**
 * Draw a box outline.
 */
void DrawBox(int col, int row, int width, int height, uint32_t color = 0xFFFFFFFF);

/**
 * Get the output as a string (instead of printing).
 */
std::string GetFrameOutput(bool useColor = true, bool useUnicode = true);

/**
 * Get a compact output scaled to fit within maxWidth columns.
 * Useful for 80-column terminals (default 78 + 2 border = 80).
 */
std::string GetCompactOutput(int maxWidth = 78);

/**
 * Get the raw text buffer without borders or colors.
 * Each row is newline-terminated.
 */
std::string GetRawText();

/**
 * Get text at a specific grid position.
 * Useful for testing that expected text appears at expected positions.
 */
std::string GetTextAt(int col, int row, int length);

/**
 * Coordinate conversion helpers.
 */
int ColToPixelX(int col);
int RowToPixelY(int row);
int GetGridWidth();
int GetGridHeight();

} // namespace AsciiRenderer
