/**
 * GX2 Overlay Rendering Implementation
 *
 * Based on the NotificationModule approach:
 * 1. Hook GX2CopyColorBufferToScanBuffer via WUPS function replacement
 * 2. Before copy, draw our overlay into the color buffer
 * 3. Restore game's GX2 context state
 */

#ifdef ENABLE_GX2_RENDERING

#include "gx2_overlay.h"
#include "SchriftGX2.h"
#include "shaders/ColorShader.h"
#include "shaders/Texture2DShader.h"

#include <cstring>
#include <malloc.h>

#include <wups.h>
#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/systeminfo.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <memory/mappedmemory.h>

#pragma GCC diagnostic ignored "-Wvolatile"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace GX2Overlay {

// State
static bool sInitialized = false;
static bool sEnabled = false;
static GX2ContextState* sContextState = nullptr;
GX2ContextState* sSavedContextState = nullptr;

// Screen dimensions
static float sScreenWidth = 1280.0f;
static float sScreenHeight = 720.0f;

// Color vertex buffer for rectangles (4 vertices, RGBA each)
static uint8_t* sColorVtxs = nullptr;
static const uint32_t COLOR_VTX_COUNT = 4;

// Font for text rendering
static SchriftGX2::Font* sDefaultFont = nullptr;

namespace {

void drawOverlay(GX2ColorBuffer* colorBuffer, GX2ScanTarget scanTarget) {
    if (!sEnabled || !sContextState) {
        return;
    }

    GX2ContextState* savedContext = sSavedContextState;
    GX2SetContextState(sContextState);

    GX2SetViewport(0, 0, colorBuffer->surface.width, colorBuffer->surface.height, 0.0f, 1.0f);
    GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
    GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
    GX2SetDepthOnlyControl(GX2_FALSE, GX2_FALSE, GX2_COMPARE_FUNC_ALWAYS);

    // Enable alpha blending
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(
        GX2_RENDER_TARGET_0,
        GX2_BLEND_MODE_SRC_ALPHA,
        GX2_BLEND_MODE_INV_SRC_ALPHA,
        GX2_BLEND_COMBINE_MODE_ADD,
        TRUE,
        GX2_BLEND_MODE_ONE,
        GX2_BLEND_MODE_INV_SRC_ALPHA,
        GX2_BLEND_COMBINE_MODE_ADD
    );

    // Drawing happens here when menu calls DrawRect/DrawText

    if (savedContext) {
        GX2SetContextState(savedContext);
    }
}

} // anonymous namespace
} // namespace GX2Overlay

// WUPS Function Replacements - DISABLED
// These hooks cause system freeze on Wii U Menu because they intercept GX2
// calls even when the overlay is disabled. The hook-based overlay approach
// needs architectural redesign to work properly for full-screen menu takeover.
#if 0
DECL_FUNCTION(void, GX2SetContextState_hook, GX2ContextState* state) {
    GX2Overlay::sSavedContextState = state;
    real_GX2SetContextState_hook(state);
}
WUPS_MUST_REPLACE(GX2SetContextState_hook, WUPS_LOADER_LIBRARY_GX2, GX2SetContextState);

DECL_FUNCTION(void, GX2CopyColorBufferToScanBuffer_hook,
              GX2ColorBuffer* colorBuffer, GX2ScanTarget scanTarget) {
    GX2Overlay::drawOverlay(colorBuffer, scanTarget);
    real_GX2CopyColorBufferToScanBuffer_hook(colorBuffer, scanTarget);
}
WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer_hook, WUPS_LOADER_LIBRARY_GX2, GX2CopyColorBufferToScanBuffer);
#endif

namespace GX2Overlay {

bool Init() {
    if (sInitialized) {
        return true;
    }

    // Allocate context state
    sContextState = (GX2ContextState*)MEMAllocFromMappedMemoryForGX2Ex(
        sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
    if (!sContextState) {
        return false;
    }
    GX2SetupContextStateEx(sContextState, GX2_TRUE);

    // Initialize shaders (singletons auto-create on first instance() call)
    ColorShader::instance();
    Texture2DShader::instance();

    // Allocate color vertex buffer
    sColorVtxs = (uint8_t*)MEMAllocFromMappedMemoryForGX2Ex(
        COLOR_VTX_COUNT * ColorShader::cuColorAttrSize, GX2_VERTEX_BUFFER_ALIGNMENT);
    if (!sColorVtxs) {
        MEMFreeToMappedMemory(sContextState);
        sContextState = nullptr;
        return false;
    }

    // Initialize font system and load default font
    SchriftGX2::Init();
    sDefaultFont = SchriftGX2::LoadDefaultFont(16.0f);

    sInitialized = true;
    sEnabled = false;
    return true;
}

void Shutdown() {
    if (!sInitialized) {
        return;
    }

    // Clean up font
    if (sDefaultFont) {
        delete sDefaultFont;
        sDefaultFont = nullptr;
    }
    SchriftGX2::Shutdown();

    if (sColorVtxs) {
        MEMFreeToMappedMemory(sColorVtxs);
        sColorVtxs = nullptr;
    }

    Texture2DShader::destroyInstance();
    ColorShader::destroyInstance();

    if (sContextState) {
        MEMFreeToMappedMemory(sContextState);
        sContextState = nullptr;
    }

    sInitialized = false;
    sEnabled = false;
}

bool IsInitialized() { return sInitialized; }
void SetEnabled(bool enabled) { sEnabled = enabled; }
bool IsEnabled() { return sEnabled; }

void SetScreenSize(float width, float height) {
    sScreenWidth = width;
    sScreenHeight = height;
    SchriftGX2::SetScreenSize(width, height);
}

void BeginDraw(uint32_t clearColor) {
    (void)clearColor;
}

void EndDraw() {
    GX2DrawDone();
}

void DrawRect(int x, int y, int width, int height, uint32_t color) {
    if (!sColorVtxs) {
        return;
    }

    // Set color for all 4 vertices (RGBA format)
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    for (int i = 0; i < 4; i++) {
        sColorVtxs[i * 4 + 0] = r;
        sColorVtxs[i * 4 + 1] = g;
        sColorVtxs[i * 4 + 2] = b;
        sColorVtxs[i * 4 + 3] = a;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sColorVtxs, COLOR_VTX_COUNT * 4);

    // Calculate offset and scale for the shader
    // Shader uses normalized device coords (-1 to 1)
    // offset is the center of the quad, scale is half-size
    float centerX = (float)x + (float)width / 2.0f;
    float centerY = (float)y + (float)height / 2.0f;

    // Convert to NDC: x: 0..screenWidth -> -1..1, y: 0..screenHeight -> 1..-1 (flipped)
    glm::vec3 offset(
        (centerX / sScreenWidth) * 2.0f - 1.0f,
        1.0f - (centerY / sScreenHeight) * 2.0f,
        0.0f
    );

    glm::vec3 scale(
        (float)width / sScreenWidth,
        (float)height / sScreenHeight,
        1.0f
    );

    glm::vec4 colorIntensity(1.0f, 1.0f, 1.0f, 1.0f);

    ColorShader::instance()->setShaders();
    ColorShader::instance()->setAttributeBuffer(sColorVtxs);
    ColorShader::instance()->setAngle(0.0f);
    ColorShader::instance()->setOffset(offset);
    ColorShader::instance()->setScale(scale);
    ColorShader::instance()->setColorIntensity(colorIntensity);
    ColorShader::instance()->draw();
}

void DrawText(int x, int y, const char* text, uint32_t color, int size) {
    (void)size;  // Font size is fixed at initialization
    if (!sDefaultFont) {
        return;
    }
    SchriftGX2::DrawText(sDefaultFont, (float)x, (float)y, text, color);
}

void DrawTexture(int x, int y, GX2Texture* texture, int width, int height) {
    // TODO: Texture rendering - Texture2DShader needs adaptation
    (void)x; (void)y; (void)texture; (void)width; (void)height;
}

int GetScreenWidth() { return (int)sScreenWidth; }
int GetScreenHeight() { return (int)sScreenHeight; }

} // namespace GX2Overlay

#endif // ENABLE_GX2_RENDERING
