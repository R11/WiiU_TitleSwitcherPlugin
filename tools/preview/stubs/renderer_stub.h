/**
 * Renderer Stub for Desktop Preview
 *
 * Provides the Renderer API using ASCII output to terminal.
 */

#pragma once

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <iostream>

namespace Renderer {

// Screen dimensions matching DRC
constexpr int DRC_WIDTH = 854;
constexpr int DRC_HEIGHT = 480;
constexpr int GRID_COLS = 100;
constexpr int GRID_ROWS = 18;
constexpr int CHAR_WIDTH = 8;
constexpr int CHAR_HEIGHT = 26;

// Image handle (not used in preview, but needed for API compatibility)
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

// Internal state
struct Cell {
    char ch;
    uint32_t fgColor;
    uint32_t bgColor;
};

struct PixelRegion {
    int x, y, w, h;
    uint32_t color;
};

// Global state (inline for header-only implementation)
inline Backend sBackend = Backend::PREVIEW_ASCII;
inline bool sInitialized = false;
inline Cell sCharBuffer[GRID_COLS * GRID_ROWS];
inline PixelRegion sPixelRegions[64];
inline int sPixelRegionCount = 0;
inline uint32_t sClearColor = 0x000000FF;

// ANSI helpers
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

// API Implementation
inline void SetBackend(Backend backend) {
    if (!sInitialized) sBackend = backend;
}

inline Backend GetBackend() { return sBackend; }

inline bool Init() {
    sInitialized = true;
    return true;
}

inline void Shutdown() {
    sInitialized = false;
}

inline bool IsInitialized() {
    return sInitialized;
}

inline void BeginFrame(uint32_t clearColor) {
    sClearColor = clearColor;
    Cell emptyCell = {' ', 0xFFFFFFFF, clearColor};
    for (int i = 0; i < GRID_COLS * GRID_ROWS; i++) {
        sCharBuffer[i] = emptyCell;
    }
    sPixelRegionCount = 0;
}

inline std::string GetFrameOutput(bool useColor = true) {
    std::ostringstream out;

    // Top border
    out << "\u250C"; // ┌
    for (int i = 0; i < GRID_COLS; i++) out << "\u2500"; // ─
    out << "\u2510\n"; // ┐

    // Content rows
    for (int row = 0; row < GRID_ROWS; row++) {
        out << "\u2502"; // │

        uint32_t lastFg = 0, lastBg = 0;
        bool colorActive = false;

        for (int col = 0; col < GRID_COLS; col++) {
            const Cell& cell = sCharBuffer[row * GRID_COLS + col];

            // Check for pixel regions
            int px = col * CHAR_WIDTH;
            int py = row * CHAR_HEIGHT;
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
    for (int i = 0; i < GRID_COLS; i++) out << "\u2500"; // ─
    out << "\u2518\n"; // ┘

    out << "Screen: DRC (GamePad) | " << DRC_WIDTH << "x" << DRC_HEIGHT
        << " pixels | " << GRID_COLS << "x" << GRID_ROWS << " chars\n";

    return out.str();
}

inline void EndFrame() {
    std::cout << GetFrameOutput(true);
}

inline void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF) {
    if (!sInitialized || !text) return;
    if (row < 0 || row >= GRID_ROWS) return;

    int len = strlen(text);
    for (int i = 0; i < len && (col + i) < GRID_COLS; i++) {
        if (col + i < 0) continue;
        int idx = row * GRID_COLS + col + i;
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
    // Just draw a placeholder since we can't render actual images in ASCII
    int w = (width > 0) ? width : 64;
    int h = (height > 0) ? height : 64;
    DrawPlaceholder(x, y, w, h, 0x808080FF);
}

inline int ColToPixelX(int col) { return col * CHAR_WIDTH; }
inline int RowToPixelY(int row) { return row * CHAR_HEIGHT; }
inline int GetScreenWidth() { return DRC_WIDTH; }
inline int GetScreenHeight() { return DRC_HEIGHT; }
inline int GetGridWidth() { return GRID_COLS; }
inline int GetGridHeight() { return GRID_ROWS; }

} // namespace Renderer
