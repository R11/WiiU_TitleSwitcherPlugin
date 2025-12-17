/**
 * GX2 Overlay Rendering Implementation
 *
 * Based on the NotificationModule approach:
 * 1. Hook GX2CopyColorBufferToScanBuffer via WUPS function replacement
 * 2. Before copy, draw our overlay into the color buffer
 * 3. Restore game's GX2 context state
 *
 * Reference: https://github.com/wiiu-env/NotificationModule
 */

// Only compile when GX2 rendering is enabled
// TODO: Enable this once shaders and font rendering are implemented
#ifdef ENABLE_GX2_RENDERING

#include "gx2_overlay.h"
#include "SchriftGX2.h"
#include "shaders/ColorShader.h"
#include "shaders/Texture2DShader.h"

#include <cstring>
#include <malloc.h>

// WUPS headers
#include <wups.h>

// WUT headers
#include <coreinit/cache.h>
#include <coreinit/memexpheap.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/texture.h>
#include <memory/mappedmemory.h>

namespace GX2Overlay {

// =============================================================================
// Internal State (namespace-level for access from WUPS hooks)
// =============================================================================

static bool sInitialized = false;
static bool sEnabled = false;

// Our GX2 context state (separate from game's)
static GX2ContextState* sContextState = nullptr;

// Saved game context state (to restore after drawing)
GX2ContextState* sSavedContextState = nullptr;  // Accessed by WUPS hooks

// Screen dimensions
static int sScreenWidth = 1280;
static int sScreenHeight = 720;

// Font for text rendering
static SchriftGX2::Font* sFont = nullptr;
static const float DEFAULT_FONT_SIZE = 16.0f;

namespace {

/**
 * Draw our overlay into the color buffer before it's copied to screen
 */
void drawOverlay(GX2ColorBuffer* colorBuffer, GX2ScanTarget scanTarget) {
    if (!sEnabled || !sContextState) {
        return;
    }

    // Save current context
    GX2ContextState* savedContext = sSavedContextState;

    // Switch to our context
    GX2SetContextState(sContextState);

    // Set up viewport to match color buffer
    GX2SetViewport(0, 0, colorBuffer->surface.width, colorBuffer->surface.height,
                   0.0f, 1.0f);
    GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);

    // Set color buffer as render target
    GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);

    // TODO: Draw overlay content here
    // - Use Texture2DShader for text/images
    // - Use ColorShader for rectangles

    // Restore game's context
    if (savedContext) {
        GX2SetContextState(savedContext);
    }
}

} // anonymous namespace

} // namespace GX2Overlay

// =============================================================================
// WUPS Function Replacements (must be in global scope)
// =============================================================================

/**
 * Hook for GX2SetContextState - captures game's context state
 */
DECL_FUNCTION(void, GX2SetContextState_hook, GX2ContextState* state) {
    // Save reference to game's context state
    GX2Overlay::sSavedContextState = state;
    real_GX2SetContextState_hook(state);
}
WUPS_MUST_REPLACE(GX2SetContextState_hook, WUPS_LOADER_LIBRARY_GX2, GX2SetContextState);

/**
 * Hook for GX2CopyColorBufferToScanBuffer - intercepts frame display
 */
DECL_FUNCTION(void, GX2CopyColorBufferToScanBuffer_hook,
              GX2ColorBuffer* colorBuffer, GX2ScanTarget scanTarget) {
    // Draw overlay before copying to screen
    GX2Overlay::drawOverlay(colorBuffer, scanTarget);

    // Call original function to display the frame
    real_GX2CopyColorBufferToScanBuffer_hook(colorBuffer, scanTarget);
}
WUPS_MUST_REPLACE(GX2CopyColorBufferToScanBuffer_hook, WUPS_LOADER_LIBRARY_GX2, GX2CopyColorBufferToScanBuffer);

// =============================================================================
// Public API (back in namespace)
// =============================================================================

namespace GX2Overlay {

bool Init() {
    if (sInitialized) {
        return true;
    }

    // Allocate our GX2 context state
    sContextState = (GX2ContextState*)MEMAllocFromMappedMemoryForGX2Ex(
        sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);

    if (!sContextState) {
        return false;
    }

    // Initialize context state
    GX2SetupContextStateEx(sContextState, GX2_TRUE);

    // Initialize shaders
    if (!ColorShader::Init()) {
        // ColorShader failed - continue anyway, DrawRect won't work
    }

    if (!Texture2DShader::Init()) {
        // Texture2DShader failed - continue anyway, DrawText won't work
    }

    // Initialize font system
    if (SchriftGX2::Init()) {
        // Load default font
        sFont = SchriftGX2::LoadDefaultFont(DEFAULT_FONT_SIZE);
    }

    // Set screen size for shaders
    ColorShader::SetScreenSize((float)sScreenWidth, (float)sScreenHeight);
    Texture2DShader::SetScreenSize((float)sScreenWidth, (float)sScreenHeight);

    sInitialized = true;
    sEnabled = false;

    return true;
}

void Shutdown() {
    if (!sInitialized) {
        return;
    }

    // Free font
    if (sFont) {
        delete sFont;
        sFont = nullptr;
    }

    // Shutdown font system
    SchriftGX2::Shutdown();

    // Shutdown shaders
    Texture2DShader::Shutdown();
    ColorShader::Shutdown();

    // Free context state
    if (sContextState) {
        MEMFreeToMappedMemory(sContextState);
        sContextState = nullptr;
    }

    sInitialized = false;
    sEnabled = false;
}

bool IsInitialized() {
    return sInitialized;
}

void SetEnabled(bool enabled) {
    sEnabled = enabled;
}

bool IsEnabled() {
    return sEnabled;
}

void BeginDraw(uint32_t clearColor) {
    // TODO: Set up for drawing
    // - Clear overlay area if needed
    // - Set blend mode for alpha
    (void)clearColor;
}

void EndDraw() {
    // TODO: Finalize drawing
    // - Flush GPU commands
}

void DrawText(int x, int y, const char* text, uint32_t color, int size) {
    if (!sFont || !Texture2DShader::IsInitialized()) {
        return;
    }

    // For now, use default font size
    // TODO: Support different sizes by loading multiple font instances
    (void)size;

    Texture2DShader::Begin();
    SchriftGX2::DrawText(sFont, (float)x, (float)y, text, color);
    Texture2DShader::End();
}

void DrawRect(int x, int y, int width, int height, uint32_t color) {
    if (!ColorShader::IsInitialized()) {
        return;
    }

    ColorShader::Begin();
    ColorShader::DrawRect((float)x, (float)y, (float)width, (float)height, color);
    ColorShader::End();
}

void DrawTexture(int x, int y, GX2Texture* texture, int width, int height) {
    if (!texture || !Texture2DShader::IsInitialized()) {
        return;
    }

    Texture2DShader::Begin();
    Texture2DShader::DrawTexture(texture, (float)x, (float)y, (float)width, (float)height, 0xFFFFFFFF);
    Texture2DShader::End();
}

int GetScreenWidth() {
    return sScreenWidth;
}

int GetScreenHeight() {
    return sScreenHeight;
}

} // namespace GX2Overlay

#endif // ENABLE_GX2_RENDERING
