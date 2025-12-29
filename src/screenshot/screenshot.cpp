/**
 * Screenshot Capture Implementation
 *
 * Captures the game's framebuffer for display in the menu.
 * The framebuffer is captured before OSScreen takes over.
 */

#include "screenshot.h"
#include "../render/renderer.h"
#include "../common/screen_constants.h"

#include <gx2/surface.h>
#include <gx2/mem.h>
#include <gx2/event.h>
#include <coreinit/cache.h>
#include <memory/mappedmemory.h>

#include <cstring>
#include <cstdlib>

namespace Screenshot {

namespace {

Renderer::ImageData* sCapturedTV = nullptr;
Renderer::ImageData* sCapturedDRC = nullptr;
int sCaptureWidth = 0;
int sCaptureHeight = 0;

void freeImageData(Renderer::ImageData*& img)
{
    if (img) {
        if (img->pixels) {
            free(img->pixels);
        }
        delete img;
        img = nullptr;
    }
}

int getWidthFromTVMode(int32_t tvMode)
{
    switch (tvMode) {
        case 4: return Screen::TV::P1080::WIDTH;  // 1080p
        case 3: return Screen::TV::P720::WIDTH;   // 720p
        case 1:
        case 2: return Screen::TV::P480::WIDTH;   // 480p
        default: return Screen::TV::P1080::WIDTH;
    }
}

int getHeightFromTVMode(int32_t tvMode)
{
    switch (tvMode) {
        case 4: return Screen::TV::P1080::HEIGHT;
        case 3: return Screen::TV::P720::HEIGHT;
        case 1:
        case 2: return Screen::TV::P480::HEIGHT;
        default: return Screen::TV::P1080::HEIGHT;
    }
}

Renderer::ImageData* captureBuffer(void* srcBuffer, uint32_t bufferSize,
                                    int width, int height, bool useGX2Copy)
{
    if (!srcBuffer || bufferSize == 0 || width <= 0 || height <= 0) {
        return nullptr;
    }

    int pixelCount = width * height;
    uint32_t* pixels = static_cast<uint32_t*>(malloc(pixelCount * sizeof(uint32_t)));
    if (!pixels) {
        return nullptr;
    }

    if (useGX2Copy) {
        // Use GX2CopySurface for tiled to linear conversion
        // Create source surface descriptor
        GX2Surface srcSurface = {};
        srcSurface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        srcSurface.width = width;
        srcSurface.height = height;
        srcSurface.depth = 1;
        srcSurface.mipLevels = 1;
        srcSurface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        srcSurface.aa = GX2_AA_MODE1X;
        srcSurface.use = GX2_SURFACE_USE_TEXTURE;
        srcSurface.tileMode = GX2_TILE_MODE_DEFAULT;
        srcSurface.image = srcBuffer;
        GX2CalcSurfaceSizeAndAlignment(&srcSurface);

        // Create destination surface with linear tile mode
        GX2Surface dstSurface = srcSurface;
        dstSurface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        GX2CalcSurfaceSizeAndAlignment(&dstSurface);

        void* linearBuffer = MEMAllocFromMappedMemoryForGX2Ex(
            dstSurface.imageSize, dstSurface.alignment);

        if (!linearBuffer) {
            free(pixels);
            return nullptr;
        }

        dstSurface.image = linearBuffer;

        // Perform GPU copy with deswizzle
        GX2CopySurface(&srcSurface, 0, 0, &dstSurface, 0, 0);
        GX2DrawDone();
        DCInvalidateRange(linearBuffer, dstSurface.imageSize);

        // Copy to our pixel buffer with proper pitch handling
        uint32_t* src = static_cast<uint32_t*>(linearBuffer);
        uint32_t pitch = dstSurface.pitch;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                pixels[y * width + x] = src[y * pitch + x];
            }
        }

        MEMFreeToMappedMemory(linearBuffer);
    } else {
        // Direct copy - assumes buffer is already in linear RGBA format
        // This may produce garbled output if the buffer is tiled
        DCInvalidateRange(srcBuffer, bufferSize);

        uint32_t* src = static_cast<uint32_t*>(srcBuffer);

        // Calculate expected pitch (may have alignment padding)
        uint32_t bytesPerRow = width * 4;
        uint32_t pitch = (bytesPerRow + 255) / 256 * 256 / 4;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint32_t srcIdx = y * pitch + x;
                if (srcIdx * 4 < bufferSize) {
                    pixels[y * width + x] = src[srcIdx];
                } else {
                    pixels[y * width + x] = 0xFF00FFFF;  // Magenta for out of bounds
                }
            }
        }
    }

    Renderer::ImageData* img = new Renderer::ImageData();
    img->pixels = pixels;
    img->width = width;
    img->height = height;

    return img;
}

}

bool CaptureFromGame()
{
    Release();

    void* tvBuffer = nullptr;
    uint32_t tvSize = 0;
    int32_t tvMode = 0;

    if (!Renderer::GetStoredTVBuffer(&tvBuffer, &tvSize, &tvMode)) {
        return false;
    }

    int width = getWidthFromTVMode(tvMode);
    int height = getHeightFromTVMode(tvMode);

    // Try GX2Copy first for proper deswizzling
    // If that fails or produces bad output, try direct copy
    sCapturedTV = captureBuffer(tvBuffer, tvSize, width, height, true);

    if (!sCapturedTV) {
        // Fallback to direct copy
        sCapturedTV = captureBuffer(tvBuffer, tvSize, width, height, false);
    }

    if (sCapturedTV) {
        sCaptureWidth = width;
        sCaptureHeight = height;
        return true;
    }

    return false;
}

Renderer::ImageHandle GetTVImage()
{
    return sCapturedTV;
}

Renderer::ImageHandle GetDRCImage()
{
    return sCapturedDRC;
}

bool HasCapture()
{
    return sCapturedTV != nullptr;
}

int GetCaptureWidth()
{
    return sCaptureWidth;
}

int GetCaptureHeight()
{
    return sCaptureHeight;
}

void Release()
{
    freeImageData(sCapturedTV);
    freeImageData(sCapturedDRC);
    sCaptureWidth = 0;
    sCaptureHeight = 0;
}

}
