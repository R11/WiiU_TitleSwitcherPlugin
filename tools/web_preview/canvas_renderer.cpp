/**
 * Canvas Renderer Backend - Dual Screen Support
 *
 * Implements the Renderer interface using HTML Canvas via Emscripten.
 * Supports simultaneous rendering to both TV and GamePad (DRC) screens.
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

// =============================================================================
// Screen Configuration
// =============================================================================

// Screen type for dual-screen rendering
enum class Screen {
    DRC,    // GamePad (always 854x480)
    TV      // TV (variable resolution)
};

// DRC dimensions (fixed)
static constexpr int DRC_WIDTH = 854;
static constexpr int DRC_HEIGHT = 480;

// TV dimensions (variable, default 720p)
static int sTvWidth = 1280;
static int sTvHeight = 720;

// Character grid dimensions (based on 8x24 character cells)
static constexpr int CHAR_WIDTH = 8;
static constexpr int CHAR_HEIGHT = 24;

// Current screen being rendered
static Screen sCurrentScreen = Screen::DRC;

// Framebuffers (RGBA format)
static uint32_t* sDrcFramebuffer = nullptr;
static uint32_t* sTvFramebuffer = nullptr;
static bool sInitialized = false;
static uint32_t sBgColor = 0x1E1E2EFF;

// =============================================================================
// Helper Functions
// =============================================================================

static int getCurrentWidth() {
    return (sCurrentScreen == Screen::DRC) ? DRC_WIDTH : sTvWidth;
}

static int getCurrentHeight() {
    return (sCurrentScreen == Screen::DRC) ? DRC_HEIGHT : sTvHeight;
}

static uint32_t* getCurrentFramebuffer() {
    return (sCurrentScreen == Screen::DRC) ? sDrcFramebuffer : sTvFramebuffer;
}

static int getGridCols() {
    return getCurrentWidth() / CHAR_WIDTH;
}

static int getGridRows() {
    return getCurrentHeight() / CHAR_HEIGHT;
}

/**
 * Set a single pixel in the current framebuffer
 */
static void setPixel(int x, int y, uint32_t color) {
    int width = getCurrentWidth();
    int height = getCurrentHeight();
    uint32_t* fb = getCurrentFramebuffer();

    if (x < 0 || x >= width || y < 0 || y >= height || !fb) return;

    // Color is RGBA, convert to ABGR for Canvas ImageData
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    fb[y * width + x] = (a << 24) | (b << 16) | (g << 8) | r;
}

/**
 * Fill a rectangle
 */
static void fillRect(int x, int y, int w, int h, uint32_t color) {
    int width = getCurrentWidth();
    int height = getCurrentHeight();

    for (int py = y; py < y + h && py < height; py++) {
        for (int px = x; px < x + w && px < width; px++) {
            if (px >= 0 && py >= 0) {
                setPixel(px, py, color);
            }
        }
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

bool Init() {
    if (sInitialized) return true;

    // Allocate DRC framebuffer
    sDrcFramebuffer = new uint32_t[DRC_WIDTH * DRC_HEIGHT];
    if (!sDrcFramebuffer) return false;
    std::memset(sDrcFramebuffer, 0, DRC_WIDTH * DRC_HEIGHT * sizeof(uint32_t));

    // Allocate TV framebuffer (max size for 1080p)
    sTvFramebuffer = new uint32_t[1920 * 1080];
    if (!sTvFramebuffer) {
        delete[] sDrcFramebuffer;
        sDrcFramebuffer = nullptr;
        return false;
    }
    std::memset(sTvFramebuffer, 0, 1920 * 1080 * sizeof(uint32_t));

    sInitialized = true;
    return true;
}

void Shutdown() {
    if (sDrcFramebuffer) {
        delete[] sDrcFramebuffer;
        sDrcFramebuffer = nullptr;
    }
    if (sTvFramebuffer) {
        delete[] sTvFramebuffer;
        sTvFramebuffer = nullptr;
    }
    sInitialized = false;
}

bool IsInitialized() {
    return sInitialized;
}

// =============================================================================
// Screen Selection
// =============================================================================

void SelectScreen(int screen) {
    sCurrentScreen = (screen == 0) ? Screen::DRC : Screen::TV;
}

void SetTvResolution(int resolution) {
    switch (resolution) {
        case 1080:
            sTvWidth = 1920;
            sTvHeight = 1080;
            break;
        case 720:
            sTvWidth = 1280;
            sTvHeight = 720;
            break;
        case 480:
        default:
            sTvWidth = 854;
            sTvHeight = 480;
            break;
    }
}

// =============================================================================
// Frame Management
// =============================================================================

void BeginFrame(uint32_t bgColor) {
    sBgColor = bgColor;
    int width = getCurrentWidth();
    int height = getCurrentHeight();

    // Fill current screen with background color
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            setPixel(x, y, bgColor);
        }
    }
}

void EndFrame() {
#ifdef __EMSCRIPTEN__
    // Push DRC framebuffer
    EM_ASM({
        const canvas = document.getElementById('screen-drc');
        if (canvas) {
            const ctx = canvas.getContext('2d');
            const imageData = ctx.createImageData($0, $1);
            const data = new Uint8ClampedArray(Module.HEAPU8.buffer, $2, $0 * $1 * 4);
            imageData.data.set(data);
            ctx.putImageData(imageData, 0, 0);
        }
    }, DRC_WIDTH, DRC_HEIGHT, sDrcFramebuffer);

    // Push TV framebuffer
    EM_ASM({
        const canvas = document.getElementById('screen-tv');
        if (canvas) {
            const ctx = canvas.getContext('2d');
            const imageData = ctx.createImageData($0, $1);
            const data = new Uint8ClampedArray(Module.HEAPU8.buffer, $2, $0 * $1 * 4);
            imageData.data.set(data);
            ctx.putImageData(imageData, 0, 0);
        }
    }, sTvWidth, sTvHeight, sTvFramebuffer);
#endif
}

// =============================================================================
// Text Rendering
// =============================================================================

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

void DrawTextF(int col, int row, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, 0xFFFFFFFF);
}

void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    DrawText(col, row, buffer, color);
}

// =============================================================================
// Image Rendering
// =============================================================================

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

void DrawImage(int x, int y, ImageHandle handle, int width, int height) {
    (void)handle;
    if (width <= 0) width = 64;
    if (height <= 0) height = 64;
    DrawPlaceholder(x, y, width, height, 0x444444FF);
}

bool SupportsImages() { return false; }

// =============================================================================
// Screen Info Functions
// =============================================================================

int GetScreenWidth() { return getCurrentWidth(); }
int GetScreenHeight() { return getCurrentHeight(); }
int GetGridWidth() { return getGridCols(); }
int GetGridHeight() { return getGridRows(); }

int ColToPixelX(int col) { return col * CHAR_WIDTH; }
int RowToPixelY(int row) { return row * CHAR_HEIGHT; }

// =============================================================================
// Layout Functions
// =============================================================================

int GetDividerCol() { return getGridCols() * 30 / 100; }
int GetDetailsPanelCol() { return GetDividerCol() + 2; }
int GetListWidth() { return GetDividerCol() - 1; }
int GetVisibleRows() { return getGridRows() - 4; }
int GetFooterRow() { return getGridRows() - 1; }

int GetTitleNameWidth(bool showNumbers) {
    int width = GetListWidth() - 4;
    if (showNumbers) {
        width -= 4;
    }
    return width;
}

// =============================================================================
// Backend Selection
// =============================================================================

void SetBackend(Backend backend) { (void)backend; }
Backend GetBackend() { return Backend::OS_SCREEN; }

} // namespace Renderer

// =============================================================================
// Exported Functions for JavaScript
// =============================================================================

extern "C" {

void selectScreen(int screen) {
    Renderer::SelectScreen(screen);
}

void setTvResolution(int resolution) {
    Renderer::SetTvResolution(resolution);
}

}
