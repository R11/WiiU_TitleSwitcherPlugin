/**
 * Renderer Stub for Desktop Preview
 *
 * Provides the Renderer API using ASCII output to terminal.
 * Supports multiple screen types with dynamic layout calculations.
 */

#pragma once

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

namespace Renderer {

// =============================================================================
// Screen Types
// =============================================================================

enum class ScreenType {
    DRC,        // GamePad: 854x480 (16:9)
    TV_1080P,   // TV HD: 1920x1080 (16:9)
    TV_720P,    // TV HD: 1280x720 (16:9)
    TV_480P     // TV SD: 640x480 (4:3)
};

struct ScreenConfig {
    const char* name;
    int pixelWidth;
    int pixelHeight;
    int gridCols;
    int gridRows;
    int charWidth;
    int charHeight;
    bool is4x3;         // 4:3 aspect ratio (affects layout)
};

// Screen configurations
// Character dimensions chosen so grid roughly matches OSScreen behavior
inline const ScreenConfig SCREEN_CONFIGS[] = {
    // DRC (GamePad) - baseline reference
    { "DRC (GamePad)", 854, 480, 100, 18, 8, 26, false },
    // TV 1080p - more space, show more content
    { "TV 1080p", 1920, 1080, 160, 36, 12, 30, false },
    // TV 720p - medium size
    { "TV 720p", 1280, 720, 120, 27, 10, 26, false },
    // TV 480p - 4:3 aspect, narrower
    { "TV 480p (4:3)", 640, 480, 80, 20, 8, 24, true }
};

// =============================================================================
// Image Handle
// =============================================================================

struct ImageData {
    uint32_t* pixels;
    int width;
    int height;
};
using ImageHandle = ImageData*;
constexpr ImageHandle INVALID_IMAGE = nullptr;

enum class Backend {
    OS_SCREEN,
    GX2,
    PREVIEW_ASCII
};

// =============================================================================
// Internal State
// =============================================================================

struct Cell {
    char ch;
    uint32_t fgColor;
    uint32_t bgColor;
};

struct PixelRegion {
    int x, y, w, h;
    uint32_t color;
};

// Global state
inline Backend sBackend = Backend::PREVIEW_ASCII;
inline bool sInitialized = false;
inline ScreenType sScreenType = ScreenType::DRC;
inline const ScreenConfig* sConfig = &SCREEN_CONFIGS[0];
inline std::vector<Cell> sCharBuffer;
inline PixelRegion sPixelRegions[64];
inline int sPixelRegionCount = 0;
inline uint32_t sClearColor = 0x000000FF;

// =============================================================================
// Screen Configuration
// =============================================================================

inline void SetScreenType(ScreenType type) {
    sScreenType = type;
    sConfig = &SCREEN_CONFIGS[static_cast<int>(type)];
}

inline ScreenType GetScreenType() { return sScreenType; }
inline const ScreenConfig& GetScreenConfig() { return *sConfig; }
inline bool Is4x3() { return sConfig->is4x3; }

// =============================================================================
// ANSI Helpers
// =============================================================================

inline std::string ansiFg(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", r, g, b);
    return buf;
}

inline std::string ansiBg(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[48;2;%d;%d;%dm", r, g, b);
    return buf;
}

inline std::string ansiReset() { return "\033[0m"; }

inline int getBrightness(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000;
}

inline const char* brightnessToUnicode(int brightness) {
    if (brightness < 32) return " ";
    if (brightness < 64) return "\u2591";  // ░
    if (brightness < 128) return "\u2592"; // ▒
    if (brightness < 192) return "\u2593"; // ▓
    return "\u2588"; // █
}

// =============================================================================
// Core API
// =============================================================================

inline void SetBackend(Backend backend) {
    if (!sInitialized) sBackend = backend;
}

inline Backend GetBackend() { return sBackend; }

inline bool Init() {
    sInitialized = true;
    sCharBuffer.resize(sConfig->gridCols * sConfig->gridRows);
    return true;
}

inline void Shutdown() {
    sInitialized = false;
    sCharBuffer.clear();
}

inline bool IsInitialized() { return sInitialized; }

inline void BeginFrame(uint32_t clearColor) {
    sClearColor = clearColor;

    // Resize buffer if screen type changed
    size_t requiredSize = sConfig->gridCols * sConfig->gridRows;
    if (sCharBuffer.size() != requiredSize) {
        sCharBuffer.resize(requiredSize);
    }

    Cell emptyCell = {' ', 0xFFFFFFFF, clearColor};
    std::fill(sCharBuffer.begin(), sCharBuffer.end(), emptyCell);
    sPixelRegionCount = 0;
}

inline std::string GetFrameOutput(bool useColor = true) {
    std::ostringstream out;

    int cols = sConfig->gridCols;
    int rows = sConfig->gridRows;

    // Top border
    out << "\u250C"; // ┌
    for (int i = 0; i < cols; i++) out << "\u2500"; // ─
    out << "\u2510\n"; // ┐

    // Content rows
    for (int row = 0; row < rows; row++) {
        out << "\u2502"; // │

        uint32_t lastFg = 0, lastBg = 0;
        bool colorActive = false;

        for (int col = 0; col < cols; col++) {
            const Cell& cell = sCharBuffer[row * cols + col];

            // Check for pixel regions
            int px = col * sConfig->charWidth;
            int py = row * sConfig->charHeight;
            uint32_t regionColor = 0;
            bool inRegion = false;

            for (int r = 0; r < sPixelRegionCount; r++) {
                const PixelRegion& region = sPixelRegions[r];
                if (px >= region.x && px < region.x + region.w &&
                    py >= region.y && py < region.y + region.h) {
                    regionColor = region.color;
                    inRegion = true;
                    break;
                }
            }

            if (inRegion) {
                if (useColor) {
                    if (!colorActive || lastBg != regionColor) {
                        out << ansiBg(regionColor);
                        lastBg = regionColor;
                        colorActive = true;
                    }
                }
                out << brightnessToUnicode(getBrightness(regionColor));
            } else {
                if (useColor) {
                    if (!colorActive || lastFg != cell.fgColor || lastBg != cell.bgColor) {
                        out << ansiFg(cell.fgColor) << ansiBg(cell.bgColor);
                        lastFg = cell.fgColor;
                        lastBg = cell.bgColor;
                        colorActive = true;
                    }
                }
                out << cell.ch;
            }
        }

        if (useColor && colorActive) {
            out << ansiReset();
        }
        out << "\u2502\n"; // │
    }

    // Bottom border
    out << "\u2514"; // └
    for (int i = 0; i < cols; i++) out << "\u2500"; // ─
    out << "\u2518\n"; // ┘

    // Screen info
    out << "Screen: " << sConfig->name
        << " | " << sConfig->pixelWidth << "x" << sConfig->pixelHeight << " pixels"
        << " | " << cols << "x" << rows << " chars";
    if (sConfig->is4x3) out << " (4:3)";
    out << "\n";

    return out.str();
}

inline void EndFrame() {
    std::cout << GetFrameOutput(true);
}

// =============================================================================
// Drawing API
// =============================================================================

inline void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF) {
    if (!sInitialized || !text) return;
    if (row < 0 || row >= sConfig->gridRows) return;

    int len = strlen(text);
    for (int i = 0; i < len && (col + i) < sConfig->gridCols; i++) {
        if (col + i < 0) continue;
        int idx = row * sConfig->gridCols + col + i;
        sCharBuffer[idx].ch = text[i];
        sCharBuffer[idx].fgColor = color;
    }
}

inline void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, color);
}

inline void DrawTextF(int col, int row, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, 0xFFFFFFFF);
}

inline bool SupportsImages() { return true; }

inline void DrawPlaceholder(int x, int y, int width, int height, uint32_t color) {
    if (sPixelRegionCount < 64) {
        sPixelRegions[sPixelRegionCount++] = {x, y, width, height, color};
    }
}

inline void DrawImage(int x, int y, ImageHandle image, int width = 0, int height = 0) {
    (void)image;
    int w = (width > 0) ? width : 64;
    int h = (height > 0) ? height : 64;
    DrawPlaceholder(x, y, w, h, 0x808080FF);
}

// =============================================================================
// Coordinate Helpers
// =============================================================================

inline int ColToPixelX(int col) { return col * sConfig->charWidth; }
inline int RowToPixelY(int row) { return row * sConfig->charHeight; }
inline int PixelXToCol(int x) { return x / sConfig->charWidth; }
inline int PixelYToRow(int y) { return y / sConfig->charHeight; }

inline int GetScreenWidth() { return sConfig->pixelWidth; }
inline int GetScreenHeight() { return sConfig->pixelHeight; }
inline int GetGridWidth() { return sConfig->gridCols; }
inline int GetGridHeight() { return sConfig->gridRows; }

// =============================================================================
// Layout Calculation Helpers
// =============================================================================

// Calculate layout dimensions based on screen size
// These return proportional values that adapt to different resolutions

// Get the divider column (splits list from details panel)
// Roughly 30% of screen width for list
inline int GetDividerCol() {
    return (sConfig->gridCols * 30) / 100;
}

// Get the details panel start column
inline int GetDetailsPanelCol() {
    return GetDividerCol() + 2;
}

// Get list width (same as divider column)
inline int GetListWidth() {
    return GetDividerCol();
}

// Get number of visible rows for title list based on screen height
// Reserve 3 rows for header/footer
inline int GetVisibleRows() {
    return sConfig->gridRows - 3;
}

// Get maximum title name width for list
inline int GetTitleNameWidth(bool showNumbers) {
    // List width minus prefix chars ("> * " = 4) and margin
    int baseWidth = GetListWidth() - 6;
    if (showNumbers) {
        // Reserve 3 chars for line numbers (e.g., "12.")
        baseWidth -= 3;
    }
    return baseWidth > 0 ? baseWidth : 10;
}

// Get footer row
inline int GetFooterRow() {
    return sConfig->gridRows - 1;
}

// Icon size scales with screen resolution
inline int GetIconSize() {
    if (sConfig->pixelHeight >= 1080) return 192;
    if (sConfig->pixelHeight >= 720) return 128;
    return 96;
}

// Icon position - placed in details panel, under title
inline int GetIconX() {
    return ColToPixelX(GetDetailsPanelCol());
}

inline int GetIconY() {
    return RowToPixelY(3);  // Row 3 (after title and separator)
}

} // namespace Renderer
