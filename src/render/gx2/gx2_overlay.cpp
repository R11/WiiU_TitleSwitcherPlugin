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

    sInitialized = true;
    sEnabled = false;

    return true;
}

void Shutdown() {
    if (!sInitialized) {
        return;
    }

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
    // TODO: Implement using SchriftGX2
    // - Look up glyphs for each character
    // - Render glyph textures with color
    (void)x;
    (void)y;
    (void)text;
    (void)color;
    (void)size;
}

void DrawRect(int x, int y, int width, int height, uint32_t color) {
    // TODO: Implement using ColorShader
    // - Set up vertex buffer with quad
    // - Set color uniform
    // - Draw
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

void DrawTexture(int x, int y, GX2Texture* texture, int width, int height) {
    // TODO: Implement using Texture2DShader
    // - Set up vertex buffer with quad
    // - Bind texture
    // - Draw
    (void)x;
    (void)y;
    (void)texture;
    (void)width;
    (void)height;
}

int GetScreenWidth() {
    return sScreenWidth;
}

int GetScreenHeight() {
    return sScreenHeight;
}

} // namespace GX2Overlay

#endif // ENABLE_GX2_RENDERING
