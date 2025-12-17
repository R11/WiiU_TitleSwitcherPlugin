/**
 * Texture2DShader - Textured Quad Rendering with Color Tint
 *
 * Shader for drawing textured quads with a color multiplier.
 * Used for text rendering where grayscale glyph textures are
 * multiplied by the text color.
 */

#pragma once

#include <cstdint>
#include <gx2/shaders.h>
#include <gx2/texture.h>

namespace Texture2DShader {

/**
 * Initialize the texture shader.
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
 * Begin drawing with the texture shader.
 */
void Begin();

/**
 * End drawing with the texture shader.
 */
void End();

/**
 * Draw a textured quad.
 * @param texture GX2 texture to draw
 * @param x Left position
 * @param y Top position
 * @param width Quad width (0 = texture width)
 * @param height Quad height (0 = texture height)
 * @param color Tint color (0xRRGGBBAA), multiplied with texture
 */
void DrawTexture(GX2Texture* texture, float x, float y, float width, float height, uint32_t color);

/**
 * Draw a textured quad with custom UV coordinates.
 * @param texture GX2 texture
 * @param x Left position
 * @param y Top position
 * @param width Quad width
 * @param height Quad height
 * @param u0, v0 Top-left UV
 * @param u1, v1 Bottom-right UV
 * @param color Tint color
 */
void DrawTextureUV(GX2Texture* texture, float x, float y, float width, float height,
                   float u0, float v0, float u1, float v1, uint32_t color);

/**
 * Set the screen resolution for coordinate transformation.
 * @param width Screen width
 * @param height Screen height
 */
void SetScreenSize(float width, float height);

} // namespace Texture2DShader
