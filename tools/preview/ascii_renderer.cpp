/**
 * ASCII Screen Preview Renderer Implementation
 */

#include "ascii_renderer.h"
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace AsciiRenderer {

// Screen configurations
static const ScreenConfig DRC_CONFIG = {
    854,    // pixelWidth
    480,    // pixelHeight
    100,    // gridCols
    18,     // gridRows
    8,      // charWidth (854/100 ≈ 8.5)
    26      // charHeight (480/18 ≈ 26.7)
};

static const ScreenConfig TV_CONFIG = {
    1920,   // pixelWidth
    1080,   // pixelHeight
    240,    // gridCols
    45,     // gridRows
    8,      // charWidth
    24      // charHeight
};

// Internal state
namespace {

Screen sScreen = Screen::DRC;
const ScreenConfig* sConfig = &DRC_CONFIG;
bool sInitialized = false;

// Character buffer (what we render text to)
struct Cell {
    char ch;
    uint32_t fgColor;
    uint32_t bgColor;
};

std::vector<Cell> sCharBuffer;

// Pixel buffer for images/placeholders (simplified - we track colored regions)
struct PixelRegion {
    int x, y, w, h;
    uint32_t color;
};
std::vector<PixelRegion> sPixelRegions;

uint32_t sClearColor = 0x000000FF;

// ANSI color code helpers
std::string ansiReset() {
    return "\033[0m";
}

std::string ansiFg(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    std::ostringstream ss;
    ss << "\033[38;2;" << r << ";" << g << ";" << b << "m";
    return ss.str();
}

std::string ansiBg(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    std::ostringstream ss;
    ss << "\033[48;2;" << r << ";" << g << ";" << b << "m";
    return ss.str();
}

// Convert brightness (0-255) to ASCII character
char brightnessToAscii(int brightness) {
    // From darkest to lightest
    static const char* chars = " .:-=+*#%@";
    int idx = (brightness * 9) / 255;
    return chars[std::min(9, std::max(0, idx))];
}

// Get Unicode block character based on brightness
const char* brightnessToUnicode(int brightness) {
    if (brightness < 32) return " ";
    if (brightness < 64) return "░";
    if (brightness < 128) return "▒";
    if (brightness < 192) return "▓";
    return "█";
}

// Calculate brightness from RGBA
int getBrightness(uint32_t rgba) {
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    return (r * 299 + g * 587 + b * 114) / 1000;
}

} // anonymous namespace

const ScreenConfig& GetScreenConfig(Screen screen) {
    return (screen == Screen::DRC) ? DRC_CONFIG : TV_CONFIG;
}

void Init(Screen screen) {
    sScreen = screen;
    sConfig = (screen == Screen::DRC) ? &DRC_CONFIG : &TV_CONFIG;
    sInitialized = true;

    // Allocate character buffer
    sCharBuffer.resize(sConfig->gridCols * sConfig->gridRows);
}

void Shutdown() {
    sCharBuffer.clear();
    sPixelRegions.clear();
    sInitialized = false;
}

void BeginFrame(uint32_t clearColor) {
    if (!sInitialized) return;

    sClearColor = clearColor;

    // Clear character buffer
    Cell emptyCell = {' ', 0xFFFFFFFF, clearColor};
    std::fill(sCharBuffer.begin(), sCharBuffer.end(), emptyCell);

    // Clear pixel regions
    sPixelRegions.clear();
}

void EndFrame(bool useColor, bool useUnicode) {
    std::cout << GetFrameOutput(useColor, useUnicode);
}

std::string GetFrameOutput(bool useColor, bool useUnicode) {
    if (!sInitialized) return "";

    std::ostringstream out;

    // Draw top border
    out << "┌";
    for (int i = 0; i < sConfig->gridCols; i++) out << "─";
    out << "┐\n";

    // Draw each row
    for (int row = 0; row < sConfig->gridRows; row++) {
        out << "│";

        uint32_t lastFg = 0;
        uint32_t lastBg = 0;
        bool colorActive = false;

        for (int col = 0; col < sConfig->gridCols; col++) {
            const Cell& cell = sCharBuffer[row * sConfig->gridCols + col];

            // Check if this cell is covered by a pixel region
            int px = col * sConfig->charWidth;
            int py = row * sConfig->charHeight;

            uint32_t regionColor = 0;
            bool inRegion = false;
            for (const auto& region : sPixelRegions) {
                if (px >= region.x && px < region.x + region.w &&
                    py >= region.y && py < region.y + region.h) {
                    regionColor = region.color;
                    inRegion = true;
                    break;
                }
            }

            if (inRegion) {
                // Render pixel region
                if (useColor) {
                    if (!colorActive || lastBg != regionColor) {
                        out << ansiBg(regionColor);
                        lastBg = regionColor;
                        colorActive = true;
                    }
                }

                if (useUnicode) {
                    out << brightnessToUnicode(getBrightness(regionColor));
                } else {
                    out << brightnessToAscii(getBrightness(regionColor));
                }
            } else {
                // Render character cell
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
        out << "│\n";
    }

    // Draw bottom border
    out << "└";
    for (int i = 0; i < sConfig->gridCols; i++) out << "─";
    out << "┘\n";

    // Add screen info
    out << "Screen: " << (sScreen == Screen::DRC ? "DRC (GamePad)" : "TV")
        << " | " << sConfig->pixelWidth << "x" << sConfig->pixelHeight << " pixels"
        << " | " << sConfig->gridCols << "x" << sConfig->gridRows << " chars\n";

    return out.str();
}

void DrawText(int col, int row, const char* text, uint32_t color) {
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

void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    DrawText(col, row, buffer, color);
}

void DrawImage(int x, int y, ImageHandle image, int width, int height) {
    if (!sInitialized || !image) return;

    int w = (width > 0) ? width : image->width;
    int h = (height > 0) ? height : image->height;

    // For simplicity, we'll just average the image color
    uint32_t avgR = 0, avgG = 0, avgB = 0;
    int count = 0;

    for (size_t i = 0; i < image->pixels.size(); i++) {
        uint32_t px = image->pixels[i];
        avgR += (px >> 24) & 0xFF;
        avgG += (px >> 16) & 0xFF;
        avgB += (px >> 8) & 0xFF;
        count++;
    }

    if (count > 0) {
        avgR /= count;
        avgG /= count;
        avgB /= count;
    }

    uint32_t avgColor = (avgR << 24) | (avgG << 16) | (avgB << 8) | 0xFF;

    sPixelRegions.push_back({x, y, w, h, avgColor});
}

void DrawPlaceholder(int x, int y, int width, int height, uint32_t color) {
    if (!sInitialized) return;
    sPixelRegions.push_back({x, y, width, height, color});
}

void DrawHLine(int col, int row, int length, uint32_t color) {
    if (!sInitialized) return;
    if (row < 0 || row >= sConfig->gridRows) return;

    for (int i = 0; i < length && (col + i) < sConfig->gridCols; i++) {
        if (col + i < 0) continue;
        int idx = row * sConfig->gridCols + col + i;
        sCharBuffer[idx].ch = '-';  // Will be rendered as ─ in unicode mode
        sCharBuffer[idx].fgColor = color;
    }
}

void DrawVLine(int col, int row, int length, uint32_t color) {
    if (!sInitialized) return;
    if (col < 0 || col >= sConfig->gridCols) return;

    for (int i = 0; i < length && (row + i) < sConfig->gridRows; i++) {
        if (row + i < 0) continue;
        int idx = (row + i) * sConfig->gridCols + col;
        sCharBuffer[idx].ch = '|';  // Will be rendered as │ in unicode mode
        sCharBuffer[idx].fgColor = color;
    }
}

void DrawBox(int col, int row, int width, int height, uint32_t color) {
    if (!sInitialized) return;

    // Corners
    auto setCell = [&](int c, int r, char ch) {
        if (c >= 0 && c < sConfig->gridCols && r >= 0 && r < sConfig->gridRows) {
            int idx = r * sConfig->gridCols + c;
            sCharBuffer[idx].ch = ch;
            sCharBuffer[idx].fgColor = color;
        }
    };

    // Top-left, top-right, bottom-left, bottom-right
    setCell(col, row, '+');
    setCell(col + width - 1, row, '+');
    setCell(col, row + height - 1, '+');
    setCell(col + width - 1, row + height - 1, '+');

    // Horizontal lines
    DrawHLine(col + 1, row, width - 2, color);
    DrawHLine(col + 1, row + height - 1, width - 2, color);

    // Vertical lines
    DrawVLine(col, row + 1, height - 2, color);
    DrawVLine(col + width - 1, row + 1, height - 2, color);
}

int ColToPixelX(int col) {
    return col * sConfig->charWidth;
}

int RowToPixelY(int row) {
    return row * sConfig->charHeight;
}

int GetGridWidth() {
    return sConfig->gridCols;
}

int GetGridHeight() {
    return sConfig->gridRows;
}

} // namespace AsciiRenderer
