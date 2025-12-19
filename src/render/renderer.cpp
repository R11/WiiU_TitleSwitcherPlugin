/**
 * Renderer Implementation
 * Dispatches to selected backend (OSScreen or GX2).
 */

#include "renderer.h"
#include "bitmap_font.h"
#include "../utils/dc.h"

#ifdef ENABLE_GX2_RENDERING
#include "gx2/gx2_overlay.h"
#endif

#include <wups.h>
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/systeminfo.h>
#include <gx2/event.h>
#include <gx2/surface.h>
#include <memory/mappedmemory.h>

#include <cstdio>
#include <cstdarg>

// Captured game buffers for fallback when memory allocation fails
struct StoredBuffer {
    void* buffer = nullptr;
    uint32_t buffer_size = 0;
    int32_t mode = 0;
    GX2SurfaceFormat surface_format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    GX2BufferingMode buffering_mode = GX2_BUFFERING_MODE_DOUBLE;
};

static StoredBuffer gStoredTVBuffer;
static StoredBuffer gStoredDRCBuffer;

// WUPS hooks to capture game's framebuffers
DECL_FUNCTION(void, GX2SetTVBuffer, void *buffer, uint32_t buffer_size,
              int32_t tv_render_mode, GX2SurfaceFormat format,
              GX2BufferingMode buffering_mode) {
    gStoredTVBuffer.buffer = buffer;
    gStoredTVBuffer.buffer_size = buffer_size;
    gStoredTVBuffer.mode = tv_render_mode;
    gStoredTVBuffer.surface_format = format;
    gStoredTVBuffer.buffering_mode = buffering_mode;
    real_GX2SetTVBuffer(buffer, buffer_size, tv_render_mode, format, buffering_mode);
}
WUPS_MUST_REPLACE(GX2SetTVBuffer, WUPS_LOADER_LIBRARY_GX2, GX2SetTVBuffer);

DECL_FUNCTION(void, GX2SetDRCBuffer, void *buffer, uint32_t buffer_size,
              uint32_t drc_mode, GX2SurfaceFormat surface_format,
              GX2BufferingMode buffering_mode) {
    gStoredDRCBuffer.buffer = buffer;
    gStoredDRCBuffer.buffer_size = buffer_size;
    gStoredDRCBuffer.mode = drc_mode;
    gStoredDRCBuffer.surface_format = surface_format;
    gStoredDRCBuffer.buffering_mode = buffering_mode;
    real_GX2SetDRCBuffer(buffer, buffer_size, drc_mode, surface_format, buffering_mode);
}
WUPS_MUST_REPLACE(GX2SetDRCBuffer, WUPS_LOADER_LIBRARY_GX2, GX2SetDRCBuffer);

namespace Renderer {

namespace {

Backend selectedBackend = Backend::OS_SCREEN;
bool isInitialized = false;

bool homeButtonWasEnabled = false;
DCRegisters savedDCRegisters;
void* tvFramebuffer = nullptr;
void* drcFramebuffer = nullptr;
uint32_t tvFramebufferSize = 0;
uint32_t drcFramebufferSize = 0;
bool usingFallbackTV = false;
bool usingFallbackDRC = false;

constexpr int OS_SCREEN_COLS = 100;
constexpr int OS_SCREEN_ROWS = 18;
constexpr int OS_SCREEN_CHAR_WIDTH = 8;
constexpr int OS_SCREEN_CHAR_HEIGHT = 24;
constexpr int DRC_WIDTH = 854;
constexpr int DRC_HEIGHT = 480;

bool initOSScreen()
{
    homeButtonWasEnabled = OSIsHomeButtonMenuEnabled();
    DCSaveRegisters(&savedDCRegisters);
    OSScreenInit();

    tvFramebufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    drcFramebufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // Reset fallback flags
    usingFallbackTV = false;
    usingFallbackDRC = false;

    // Try to allocate fresh buffers
    tvFramebuffer = MEMAllocFromMappedMemoryForGX2Ex(tvFramebufferSize, 0x100);
    drcFramebuffer = MEMAllocFromMappedMemoryForGX2Ex(drcFramebufferSize, 0x100);

    // Fallback to captured game buffers if allocation failed
    if (!tvFramebuffer && gStoredTVBuffer.buffer &&
        gStoredTVBuffer.buffer_size >= tvFramebufferSize) {
        tvFramebuffer = gStoredTVBuffer.buffer;
        usingFallbackTV = true;
    }

    if (!drcFramebuffer && gStoredDRCBuffer.buffer &&
        gStoredDRCBuffer.buffer_size >= drcFramebufferSize) {
        drcFramebuffer = gStoredDRCBuffer.buffer;
        usingFallbackDRC = true;
    }

    // If still no buffers, clean up and fail
    if (!tvFramebuffer || !drcFramebuffer) {
        if (tvFramebuffer && !usingFallbackTV) {
            MEMFreeToMappedMemory(tvFramebuffer);
        }
        tvFramebuffer = nullptr;
        if (drcFramebuffer && !usingFallbackDRC) {
            MEMFreeToMappedMemory(drcFramebuffer);
        }
        drcFramebuffer = nullptr;
        usingFallbackTV = false;
        usingFallbackDRC = false;
        DCRestoreRegisters(&savedDCRegisters);
        return false;
    }

    OSScreenSetBufferEx(SCREEN_TV, tvFramebuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcFramebuffer);

    for (int bufferIndex = 0; bufferIndex < 2; bufferIndex++) {
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);
        DCFlushRange(tvFramebuffer, tvFramebufferSize);
        DCFlushRange(drcFramebuffer, drcFramebufferSize);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);
    OSEnableHomeButtonMenu(false);

    return true;
}

void shutdownOSScreen()
{
    OSEnableHomeButtonMenu(homeButtonWasEnabled);
    DCRestoreRegisters(&savedDCRegisters);

    // Only free buffers we allocated (not fallback buffers borrowed from game)
    if (tvFramebuffer && !usingFallbackTV) {
        MEMFreeToMappedMemory(tvFramebuffer);
    }
    tvFramebuffer = nullptr;

    if (drcFramebuffer && !usingFallbackDRC) {
        MEMFreeToMappedMemory(drcFramebuffer);
    }
    drcFramebuffer = nullptr;

    tvFramebufferSize = 0;
    drcFramebufferSize = 0;
    usingFallbackTV = false;
    usingFallbackDRC = false;
}

void beginFrameOSScreen(uint32_t clearColor)
{
    GX2WaitForVsync();
    OSScreenClearBufferEx(SCREEN_TV, clearColor);
    OSScreenClearBufferEx(SCREEN_DRC, clearColor);
}

void endFrameOSScreen()
{
    DCFlushRange(tvFramebuffer, tvFramebufferSize);
    DCFlushRange(drcFramebuffer, drcFramebufferSize);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void drawTextOSScreen(int column, int row, const char* text, uint32_t color)
{
    // TODO: Bitmap font rendering for colored text needs visual improvements.
    // For now, use the built-in OSScreen font for white or unset colors.
    if (color == 0xFFFFFFFF || color == 0) {
        OSScreenPutFontEx(SCREEN_TV, column, row, text);
        OSScreenPutFontEx(SCREEN_DRC, column, row, text);
        return;
    }

    // For colored text, render using our bitmap font pixel-by-pixel
    // Convert color from RGBA to RGBX format for OSScreenPutPixelEx
    uint32_t rgbx = color & 0xFFFFFF00;

    // Calculate pixel position from character grid position
    int baseX = column * OS_SCREEN_CHAR_WIDTH;
    int baseY = row * OS_SCREEN_CHAR_HEIGHT;

    // Render each character
    for (int i = 0; text[i] != '\0'; i++) {
        const uint8_t* glyph = BitmapFont::GetGlyph(text[i]);
        if (!glyph) {
            // Unknown character - skip
            baseX += BitmapFont::CHAR_WIDTH;
            continue;
        }

        // Draw each pixel of the glyph
        for (int gy = 0; gy < BitmapFont::CHAR_HEIGHT; gy++) {
            for (int gx = 0; gx < BitmapFont::CHAR_WIDTH; gx++) {
                if (BitmapFont::IsPixelSet(glyph, gx, gy)) {
                    int px = baseX + gx;
                    int py = baseY + gy;
                    OSScreenPutPixelEx(SCREEN_TV, px, py, rgbx);
                    OSScreenPutPixelEx(SCREEN_DRC, px, py, rgbx);
                }
            }
        }

        baseX += BitmapFont::CHAR_WIDTH;
    }
}

void drawImageOSScreen(int pixelX, int pixelY, ImageHandle image, int targetWidth, int targetHeight)
{
    if (!image || !image->pixels) {
        return;
    }

    int sourceWidth = image->width;
    int sourceHeight = image->height;
    int destWidth = (targetWidth > 0) ? targetWidth : sourceWidth;
    int destHeight = (targetHeight > 0) ? targetHeight : sourceHeight;

    for (int destY = 0; destY < destHeight; destY++) {
        int sourceY = (destY * sourceHeight) / destHeight;
        for (int destX = 0; destX < destWidth; destX++) {
            int sourceX = (destX * sourceWidth) / destWidth;

            uint32_t pixel = image->pixels[sourceY * sourceWidth + sourceX];
            uint32_t rgbxPixel = pixel & 0xFFFFFF00;

            OSScreenPutPixelEx(SCREEN_TV, pixelX + destX, pixelY + destY, rgbxPixel);
            OSScreenPutPixelEx(SCREEN_DRC, pixelX + destX, pixelY + destY, rgbxPixel);
        }
    }
}

void drawPlaceholderOSScreen(int pixelX, int pixelY, int width, int height, uint32_t color)
{
    uint32_t rgbxColor = color & 0xFFFFFF00;

    for (int offsetY = 0; offsetY < height; offsetY++) {
        for (int offsetX = 0; offsetX < width; offsetX++) {
            OSScreenPutPixelEx(SCREEN_TV, pixelX + offsetX, pixelY + offsetY, rgbxColor);
            OSScreenPutPixelEx(SCREEN_DRC, pixelX + offsetX, pixelY + offsetY, rgbxColor);
        }
    }
}

bool initGX2()
{
#ifdef ENABLE_GX2_RENDERING
    if (!GX2Overlay::Init()) {
        return false;
    }
    GX2Overlay::SetEnabled(true);
    return true;
#else
    return false;
#endif
}

void shutdownGX2()
{
#ifdef ENABLE_GX2_RENDERING
    GX2Overlay::SetEnabled(false);
    GX2Overlay::Shutdown();
#endif
}

void beginFrameGX2(uint32_t clearColor)
{
#ifdef ENABLE_GX2_RENDERING
    GX2Overlay::BeginDraw(clearColor);
#else
    (void)clearColor;
#endif
}

void endFrameGX2()
{
#ifdef ENABLE_GX2_RENDERING
    GX2Overlay::EndDraw();
#endif
}

void drawTextGX2(int column, int row, const char* text, uint32_t color)
{
#ifdef ENABLE_GX2_RENDERING
    // Convert column/row to pixels (OSScreen uses 8x24 character cells)
    int pixelX = column * 8;
    int pixelY = row * 24;
    GX2Overlay::DrawText(pixelX, pixelY, text, color, 16);
#else
    (void)column; (void)row; (void)text; (void)color;
#endif
}

void drawImageGX2(int pixelX, int pixelY, ImageHandle image, int width, int height)
{
#ifdef ENABLE_GX2_RENDERING
    // TODO: Image rendering not yet implemented
    (void)pixelX; (void)pixelY; (void)image; (void)width; (void)height;
#else
    (void)pixelX; (void)pixelY; (void)image; (void)width; (void)height;
#endif
}

}

void SetBackend(Backend backend)
{
    if (!isInitialized) {
        selectedBackend = backend;
    }
}

Backend GetBackend()
{
    return selectedBackend;
}

bool Init()
{
    if (isInitialized) {
        return true;
    }

    bool result = false;

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            result = initOSScreen();
            break;
        case Backend::GX2:
            result = initGX2();
            break;
    }

    isInitialized = result;
    return result;
}

void Shutdown()
{
    if (!isInitialized) {
        return;
    }

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            shutdownOSScreen();
            break;
        case Backend::GX2:
            shutdownGX2();
            break;
    }

    isInitialized = false;
}

bool IsInitialized()
{
    return isInitialized;
}

void BeginFrame(uint32_t clearColor)
{
    if (!isInitialized) {
        return;
    }

    switch (selectedBackend) {
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
    if (!isInitialized) {
        return;
    }

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            endFrameOSScreen();
            break;
        case Backend::GX2:
            endFrameGX2();
            break;
    }
}

void DrawText(int column, int row, const char* text, uint32_t color)
{
    if (!isInitialized || !text) {
        return;
    }

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            drawTextOSScreen(column, row, text, 0xFFFFFFFF);
            break;
        case Backend::GX2:
            drawTextGX2(column, row, text, color);
            break;
    }
}

void DrawTextF(int column, int row, uint32_t color, const char* format, ...)
{
    if (!isInitialized || !format) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    DrawText(column, row, buffer, color);
}

void DrawTextF(int column, int row, const char* format, ...)
{
    if (!isInitialized || !format) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    DrawText(column, row, buffer, 0xFFFFFFFF);
}

bool SupportsImages()
{
    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            return true;
        case Backend::GX2:
            return true;
        default:
            return false;
    }
}

void DrawImage(int pixelX, int pixelY, ImageHandle image, int width, int height)
{
    if (!isInitialized || !SupportsImages() || !image) {
        return;
    }

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            drawImageOSScreen(pixelX, pixelY, image, width, height);
            break;
        case Backend::GX2:
            drawImageGX2(pixelX, pixelY, image, width, height);
            break;
    }
}

void DrawPlaceholder(int pixelX, int pixelY, int width, int height, uint32_t color)
{
    if (!isInitialized) {
        return;
    }

    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            drawPlaceholderOSScreen(pixelX, pixelY, width, height, color);
            break;
        case Backend::GX2:
            break;
    }
}

int ColToPixelX(int column)
{
    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            return column * OS_SCREEN_CHAR_WIDTH;
        case Backend::GX2:
            return column * OS_SCREEN_CHAR_WIDTH;
        default:
            return 0;
    }
}

int RowToPixelY(int row)
{
    switch (selectedBackend) {
        case Backend::OS_SCREEN:
            return row * OS_SCREEN_CHAR_HEIGHT;
        case Backend::GX2:
            return row * OS_SCREEN_CHAR_HEIGHT;
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

int GetDividerCol()
{
    return (GetGridWidth() * 30) / 100;
}

int GetDetailsPanelCol()
{
    return GetDividerCol() + 2;
}

int GetListWidth()
{
    return GetDividerCol();
}

int GetVisibleRows()
{
    return GetGridHeight() - 3;
}

int GetFooterRow()
{
    return GetGridHeight() - 1;
}

int GetTitleNameWidth(bool showLineNumbers)
{
    int baseWidth = GetListWidth() - 6;
    if (showLineNumbers) {
        baseWidth -= 3;
    }
    return baseWidth > 0 ? baseWidth : 10;
}

}
