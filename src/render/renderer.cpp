/**
 * Renderer Implementation
 *
 * This file implements the abstract Renderer interface by dispatching
 * to the selected backend (OSScreen or GX2).
 *
 * Currently only OSScreen is implemented. GX2 is a placeholder for future work.
 */

#include "renderer.h"
#include "../utils/dc.h"

// Wii U SDK headers
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/systeminfo.h>
#include <gx2/event.h>
#include <memory/mappedmemory.h>

// Standard library
#include <cstdio>
#include <cstdarg>

namespace Renderer {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Selected backend
Backend sBackend = Backend::OS_SCREEN;

// Is the renderer initialized?
bool sInitialized = false;

// =============================================================================
// OSScreen Backend State
// =============================================================================

// Was HOME button menu enabled before we took over?
bool sHomeButtonWasEnabled = false;

// Saved display controller registers
DCRegisters sSavedDCRegs;

// Framebuffer pointers
void* sBufferTV = nullptr;
void* sBufferDRC = nullptr;

// Framebuffer sizes
uint32_t sBufferSizeTV = 0;
uint32_t sBufferSizeDRC = 0;

// OSScreen character grid dimensions (approximate)
// DRC: ~100 columns x 18 rows at standard resolution
constexpr int OS_SCREEN_COLS = 100;
constexpr int OS_SCREEN_ROWS = 18;
constexpr int OS_SCREEN_CHAR_WIDTH = 8;   // Approximate pixels per char
constexpr int OS_SCREEN_CHAR_HEIGHT = 24; // Approximate pixels per char

// Screen dimensions (DRC)
constexpr int DRC_WIDTH = 854;
constexpr int DRC_HEIGHT = 480;

// =============================================================================
// OSScreen Backend Implementation
// =============================================================================

bool initOSScreen()
{
    // Save HOME button state
    sHomeButtonWasEnabled = OSIsHomeButtonMenuEnabled();

    // Save DC registers
    DCSaveRegisters(&sSavedDCRegs);

    // Initialize OSScreen
    OSScreenInit();

    // Get buffer sizes
    sBufferSizeTV = OSScreenGetBufferSizeEx(SCREEN_TV);
    sBufferSizeDRC = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // Allocate GX2-compatible framebuffers
    sBufferTV = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeTV, 0x100);
    sBufferDRC = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeDRC, 0x100);

    if (!sBufferTV || !sBufferDRC) {
        if (sBufferTV) {
            MEMFreeToMappedMemory(sBufferTV);
            sBufferTV = nullptr;
        }
        if (sBufferDRC) {
            MEMFreeToMappedMemory(sBufferDRC);
            sBufferDRC = nullptr;
        }
        DCRestoreRegisters(&sSavedDCRegs);
        return false;
    }

    // Set up screen buffers
    OSScreenSetBufferEx(SCREEN_TV, sBufferTV);
    OSScreenSetBufferEx(SCREEN_DRC, sBufferDRC);

    // Clear both front and back buffers
    for (int i = 0; i < 2; i++) {
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);
        DCFlushRange(sBufferTV, sBufferSizeTV);
        DCFlushRange(sBufferDRC, sBufferSizeDRC);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    // Enable screens
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    // Disable HOME button menu
    OSEnableHomeButtonMenu(false);

    return true;
}

void shutdownOSScreen()
{
    OSEnableHomeButtonMenu(sHomeButtonWasEnabled);
    DCRestoreRegisters(&sSavedDCRegs);

    if (sBufferTV) {
        MEMFreeToMappedMemory(sBufferTV);
        sBufferTV = nullptr;
    }
    if (sBufferDRC) {
        MEMFreeToMappedMemory(sBufferDRC);
        sBufferDRC = nullptr;
    }

    sBufferSizeTV = 0;
    sBufferSizeDRC = 0;
}

void beginFrameOSScreen(uint32_t clearColor)
{
    GX2WaitForVsync();
    OSScreenClearBufferEx(SCREEN_TV, clearColor);
    OSScreenClearBufferEx(SCREEN_DRC, clearColor);
}

void endFrameOSScreen()
{
    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void drawTextOSScreen(int col, int row, const char* text, uint32_t color)
{
    // Note: OSScreen doesn't support colored text directly
    // Color parameter is ignored in this implementation
    (void)color;

    OSScreenPutFontEx(SCREEN_TV, col, row, text);
    OSScreenPutFontEx(SCREEN_DRC, col, row, text);
}

void drawImageOSScreen(int x, int y, ImageHandle image, int width, int height)
{
    if (!image || !image->pixels) {
        return;
    }

    // Use original dimensions if not specified
    int srcW = image->width;
    int srcH = image->height;
    int dstW = (width > 0) ? width : srcW;
    int dstH = (height > 0) ? height : srcH;

    // Draw pixel by pixel
    // Simple nearest-neighbor scaling
    for (int dy = 0; dy < dstH; dy++) {
        int sy = (dy * srcH) / dstH;
        for (int dx = 0; dx < dstW; dx++) {
            int sx = (dx * srcW) / dstW;

            uint32_t pixel = image->pixels[sy * srcW + sx];

            // Convert from RGBA (0xRRGGBBAA) to RGBX (0xRRGGBBXX)
            // OSScreenPutPixelEx uses RGBX format
            uint32_t rgbx = pixel & 0xFFFFFF00;

            // Draw to both screens
            OSScreenPutPixelEx(SCREEN_TV, x + dx, y + dy, rgbx);
            OSScreenPutPixelEx(SCREEN_DRC, x + dx, y + dy, rgbx);
        }
    }
}

void drawPlaceholderOSScreen(int x, int y, int width, int height, uint32_t color)
{
    // Draw a filled rectangle using pixels
    uint32_t rgbx = color & 0xFFFFFF00;

    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            OSScreenPutPixelEx(SCREEN_TV, x + dx, y + dy, rgbx);
            OSScreenPutPixelEx(SCREEN_DRC, x + dx, y + dy, rgbx);
        }
    }
}

// =============================================================================
// GX2 Backend Implementation (Placeholder)
// =============================================================================

bool initGX2()
{
    // TODO: Implement GX2 initialization
    // - Set up GX2 context
    // - Load shaders
    // - Create render targets
    // - Initialize texture samplers
    return false;
}

void shutdownGX2()
{
    // TODO: Implement GX2 shutdown
}

void beginFrameGX2(uint32_t clearColor)
{
    (void)clearColor;
    // TODO: Implement GX2 frame begin
}

void endFrameGX2()
{
    // TODO: Implement GX2 frame end
}

void drawTextGX2(int col, int row, const char* text, uint32_t color)
{
    (void)col;
    (void)row;
    (void)text;
    (void)color;
    // TODO: Implement GX2 text rendering (using FreeType or similar)
}

void drawImageGX2(int x, int y, ImageHandle image, int width, int height)
{
    (void)x;
    (void)y;
    (void)image;
    (void)width;
    (void)height;
    // TODO: Implement GX2 image rendering
}

} // anonymous namespace

// =============================================================================
// Public API Implementation
// =============================================================================

void SetBackend(Backend backend)
{
    if (!sInitialized) {
        sBackend = backend;
    }
    // Cannot change backend while initialized
}

Backend GetBackend()
{
    return sBackend;
}

bool Init()
{
    if (sInitialized) {
        return true;
    }

    bool result = false;

    switch (sBackend) {
        case Backend::OS_SCREEN:
            result = initOSScreen();
            break;
        case Backend::GX2:
            result = initGX2();
            break;
    }

    sInitialized = result;
    return result;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            shutdownOSScreen();
            break;
        case Backend::GX2:
            shutdownGX2();
            break;
    }

    sInitialized = false;
}

bool IsInitialized()
{
    return sInitialized;
}

void BeginFrame(uint32_t clearColor)
{
    if (!sInitialized) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            beginFrameOSScreen(clearColor);
            break;
        case Backend::GX2:
            beginFrameGX2(clearColor);
            break;
    }
}

void EndFrame()
{
    if (!sInitialized) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            endFrameOSScreen();
            break;
        case Backend::GX2:
            endFrameGX2();
            break;
    }
}

void DrawText(int col, int row, const char* text, uint32_t color)
{
    if (!sInitialized || !text) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            drawTextOSScreen(col, row, text, color);
            break;
        case Backend::GX2:
            drawTextGX2(col, row, text, color);
            break;
    }
}

void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...)
{
    if (!sInitialized || !fmt) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    DrawText(col, row, buffer, color);
}

void DrawTextF(int col, int row, const char* fmt, ...)
{
    if (!sInitialized || !fmt) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    DrawText(col, row, buffer, 0xFFFFFFFF);
}

bool SupportsImages()
{
    switch (sBackend) {
        case Backend::OS_SCREEN:
            return true;   // OSScreen now supports images via OSScreenPutPixelEx
        case Backend::GX2:
            return true;   // GX2 supports images
        default:
            return false;
    }
}

void DrawImage(int x, int y, ImageHandle image, int width, int height)
{
    if (!sInitialized || !SupportsImages() || !image) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            drawImageOSScreen(x, y, image, width, height);
            break;
        case Backend::GX2:
            drawImageGX2(x, y, image, width, height);
            break;
    }
}

void DrawPlaceholder(int x, int y, int width, int height, uint32_t color)
{
    if (!sInitialized) {
        return;
    }

    switch (sBackend) {
        case Backend::OS_SCREEN:
            drawPlaceholderOSScreen(x, y, width, height, color);
            break;
        case Backend::GX2:
            // TODO: Draw colored rectangle
            break;
    }
}

int ColToPixelX(int col)
{
    switch (sBackend) {
        case Backend::OS_SCREEN:
            return col * OS_SCREEN_CHAR_WIDTH;
        case Backend::GX2:
            return col * OS_SCREEN_CHAR_WIDTH;  // Use same for now
        default:
            return 0;
    }
}

int RowToPixelY(int row)
{
    switch (sBackend) {
        case Backend::OS_SCREEN:
            return row * OS_SCREEN_CHAR_HEIGHT;
        case Backend::GX2:
            return row * OS_SCREEN_CHAR_HEIGHT;  // Use same for now
        default:
            return 0;
    }
}

int GetScreenWidth()
{
    return DRC_WIDTH;
}

int GetScreenHeight()
{
    return DRC_HEIGHT;
}

int GetGridWidth()
{
    return OS_SCREEN_COLS;
}

int GetGridHeight()
{
    return OS_SCREEN_ROWS;
}

} // namespace Renderer
