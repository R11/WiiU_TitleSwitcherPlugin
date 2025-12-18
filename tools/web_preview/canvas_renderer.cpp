/**
 * Canvas Renderer Backend
 *
 * Implements the Renderer interface using HTML Canvas via Emscripten.
 * Draws to a pixel buffer which is then copied to the canvas each frame.
 */

#include "render/renderer.h"
#include "render/bitmap_font.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

namespace Renderer {

// Screen dimensions (DRC resolution)
static constexpr int SCREEN_WIDTH = 854;
static constexpr int SCREEN_HEIGHT = 480;

// Character grid dimensions
static constexpr int CHAR_WIDTH = 8;
static constexpr int CHAR_HEIGHT = 24;  // OSScreen uses 24px row height
static constexpr int GRID_COLS = SCREEN_WIDTH / CHAR_WIDTH;    // ~106
static constexpr int GRID_ROWS = SCREEN_HEIGHT / CHAR_HEIGHT;  // 20

// Framebuffer (RGBA format)
static uint32_t* sFramebuffer = nullptr;
static bool sInitialized = false;
static uint32_t sBgColor = 0x1E1E2EFF;

/**
 * Initialize the renderer
 */
bool Init() {
    if (sInitialized) return true;

    sFramebuffer = new uint32_t[SCREEN_WIDTH * SCREEN_HEIGHT];
    if (!sFramebuffer) return false;

    std::memset(sFramebuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));
    sInitialized = true;
    return true;
}

/**
 * Shutdown the renderer
 */
void Shutdown() {
    if (sFramebuffer) {
        delete[] sFramebuffer;
        sFramebuffer = nullptr;
    }
    sInitialized = false;
}

/**
 * Check if initialized
 */
bool IsInitialized() {
    return sInitialized;
}

/**
 * Set a single pixel in the framebuffer
 */
static void setPixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;

    // Color is RGBA, need to convert for Canvas (which expects ABGR in memory)
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Store as ABGR for Canvas ImageData
    sFramebuffer[y * SCREEN_WIDTH + x] = (a << 24) | (b << 16) | (g << 8) | r;
}

/**
 * Fill a rectangle
 */
static void fillRect(int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h && py < SCREEN_HEIGHT; py++) {
        for (int px = x; px < x + w && px < SCREEN_WIDTH; px++) {
            if (px >= 0 && py >= 0) {
                setPixel(px, py, color);
            }
        }
    }
}

/**
 * Begin a new frame
 */
void BeginFrame(uint32_t bgColor) {
    sBgColor = bgColor;
    // Fill entire screen with background color
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            setPixel(x, y, bgColor);
        }
    }
}

/**
 * End frame and display
 */
void EndFrame() {
#ifdef __EMSCRIPTEN__
    // Push framebuffer to Canvas
    EM_ASM({
        const canvas = document.getElementById('screen');
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        const imageData = ctx.createImageData($0, $1);
        const data = new Uint8ClampedArray(Module.HEAPU8.buffer, $2, $0 * $1 * 4);
        imageData.data.set(data);
        ctx.putImageData(imageData, 0, 0);
    }, SCREEN_WIDTH, SCREEN_HEIGHT, sFramebuffer);
#endif
}

/**
 * Draw text using the bitmap font
 */
void DrawText(int col, int row, const char* text, uint32_t color) {
    if (!sInitialized || !text) return;

    int x = col * CHAR_WIDTH;
    int y = row * CHAR_HEIGHT + 8;  // Offset to center in row

    for (int i = 0; text[i] != '\0'; i++) {
        const uint8_t* glyph = BitmapFont::GetGlyph(text[i]);
        if (glyph) {
            for (int py = 0; py < BitmapFont::CHAR_HEIGHT; py++) {
                for (int px = 0; px < BitmapFont::CHAR_WIDTH; px++) {
                    if (BitmapFont::IsPixelSet(glyph, px, py)) {
                        setPixel(x + px, y + py, color);
                    }
                }
            }
        }
        x += CHAR_WIDTH;
    }
}

/**
 * Draw formatted text
 */
void DrawTextF(int col, int row, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, 0xFFFFFFFF);
}

/**
 * Draw formatted text with color
 */
void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, color);
}

/**
 * Draw a placeholder rectangle (for images)
 */
void DrawPlaceholder(int x, int y, int width, int height, uint32_t color) {
    fillRect(x, y, width, height, color);

    // Draw border
    uint32_t borderColor = 0x666666FF;
    for (int px = x; px < x + width; px++) {
        setPixel(px, y, borderColor);
        setPixel(px, y + height - 1, borderColor);
    }
    for (int py = y; py < y + height; py++) {
        setPixel(x, py, borderColor);
        setPixel(x + width - 1, py, borderColor);
    }
}

/**
 * Draw image - for web preview, just draw placeholder
 */
void DrawImage(int x, int y, ImageHandle handle, int width, int height) {
    // In web preview, we don't have actual images yet
    // Just draw a colored placeholder
    (void)handle;
    if (width <= 0) width = 64;
    if (height <= 0) height = 64;
    DrawPlaceholder(x, y, width, height, 0x444444FF);
}

// Image support stubs
bool SupportsImages() { return false; }

// Screen info functions
int GetScreenWidth() { return SCREEN_WIDTH; }
int GetScreenHeight() { return SCREEN_HEIGHT; }
int GetGridWidth() { return GRID_COLS; }
int GetGridHeight() { return GRID_ROWS; }

// Coordinate conversion
int ColToPixelX(int col) { return col * CHAR_WIDTH; }
int RowToPixelY(int row) { return row * CHAR_HEIGHT; }

// Layout functions (matching OSScreen layout)
int GetDividerCol() { return GRID_COLS * 30 / 100; }  // 30% from left
int GetDetailsPanelCol() { return GetDividerCol() + 2; }
int GetListWidth() { return GetDividerCol() - 1; }
int GetVisibleRows() { return GRID_ROWS - 4; }  // Header + footer
int GetFooterRow() { return GRID_ROWS - 1; }
int GetTitleNameWidth(bool showNumbers) {
    int width = GetListWidth() - 4;  // Base width minus prefix
    if (showNumbers) {
        width -= 4;  // Reserve space for line numbers "001."
    }
    return width;
}

// Backend selection (only Canvas for web)
void SetBackend(Backend backend) { (void)backend; }
Backend GetBackend() { return Backend::OS_SCREEN; }

} // namespace Renderer
