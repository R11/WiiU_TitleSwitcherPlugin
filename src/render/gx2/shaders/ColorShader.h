/**
 * ColorShader - Solid Color Rectangle Rendering
 *
 * Simple shader for drawing solid colored rectangles.
 * Vertex shader transforms 2D positions, pixel shader outputs flat color.
 */

#pragma once

#include <cstdint>
#include <gx2/shaders.h>
#include <gx2/texture.h>

namespace ColorShader {

/**
 * Initialize the color shader.
 * @return true on success
 */
bool Init();

/**
 * Shutdown and free shader resources.
 */
void Shutdown();

/**
 * Check if shader is initialized.
 */
bool IsInitialized();

/**
 * Begin drawing with the color shader.
 * Sets up shader state for subsequent draw calls.
 */
void Begin();

/**
 * End drawing with the color shader.
 */
void End();

/**
 * Draw a colored rectangle.
 * Must be called between Begin() and End().
 * @param x Left position
 * @param y Top position
 * @param width Rectangle width
 * @param height Rectangle height
 * @param color RGBA color (0xRRGGBBAA)
 */
void DrawRect(float x, float y, float width, float height, uint32_t color);

/**
 * Set the screen resolution for coordinate transformation.
 * @param width Screen width in pixels
 * @param height Screen height in pixels
 */
void SetScreenSize(float width, float height);

} // namespace ColorShader
