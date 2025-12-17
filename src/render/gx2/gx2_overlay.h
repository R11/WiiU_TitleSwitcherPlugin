/**
 * GX2 Overlay Rendering
 *
 * Provides GPU-accelerated overlay rendering on top of game graphics.
 * Uses libfunctionpatcher to hook GX2CopyColorBufferToScanBuffer.
 */

#pragma once

#include <cstdint>
#include <gx2/texture.h>
#include <gx2/context.h>

namespace GX2Overlay {

/**
 * Initialize the GX2 overlay system.
 * Must be called before any rendering.
 * @return true on success
 */
bool Init();

/**
 * Shutdown the GX2 overlay system.
 * Frees all resources and unhooks functions.
 */
void Shutdown();

/**
 * Check if the overlay system is initialized.
 */
bool IsInitialized();

/**
 * Set whether the overlay should be drawn.
 * @param enabled true to draw overlay, false to disable
 */
void SetEnabled(bool enabled);

/**
 * Check if overlay drawing is enabled.
 */
bool IsEnabled();

/**
 * Begin drawing to the overlay.
 * Call before issuing draw commands.
 * @param clearColor Background color (RGBA)
 */
void BeginDraw(uint32_t clearColor = 0x00000000);

/**
 * End drawing to the overlay.
 * Call after all draw commands are issued.
 */
void EndDraw();

/**
 * Draw text at the specified position.
 * @param x X position in pixels
 * @param y Y position in pixels
 * @param text Text to draw
 * @param color Text color (RGBA)
 * @param size Font size in pixels
 */
void DrawText(int x, int y, const char* text, uint32_t color, int size = 16);

/**
 * Draw a filled rectangle.
 * @param x X position
 * @param y Y position
 * @param width Rectangle width
 * @param height Rectangle height
 * @param color Fill color (RGBA)
 */
void DrawRect(int x, int y, int width, int height, uint32_t color);

/**
 * Draw a texture.
 * @param x X position
 * @param y Y position
 * @param texture GX2 texture to draw
 * @param width Display width (0 = texture width)
 * @param height Display height (0 = texture height)
 */
void DrawTexture(int x, int y, GX2Texture* texture, int width = 0, int height = 0);

/**
 * Get the screen width in pixels.
 */
int GetScreenWidth();

/**
 * Get the screen height in pixels.
 */
int GetScreenHeight();

} // namespace GX2Overlay
