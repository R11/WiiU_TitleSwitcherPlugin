/**
 * Shared Glyph Rendering Template
 *
 * Extracts the common text rendering algorithm used by both
 * OSScreen (Wii U) and Canvas (web preview) backends.
 */

#pragma once

#include "bitmap_font.h"

namespace GlyphRenderer {

/**
 * Render text using the bitmap font.
 *
 * @param baseX Starting X position in pixels
 * @param baseY Starting Y position in pixels
 * @param text Null-terminated string to render
 * @param charWidth Character cell width in pixels
 * @param charHeight Character cell height in pixels
 * @param scaleY Vertical scale factor (typically 2 for 8x8 font in 8x24 cell)
 * @param writePixel Callback to write a pixel: void(int x, int y)
 */
template<typename PixelWriter>
void RenderText(int baseX, int baseY, const char* text,
                int charWidth, int charHeight, int scaleY,
                PixelWriter&& writePixel) {
    int scaledHeight = BitmapFont::CHAR_HEIGHT * scaleY;
    int yOffset = (charHeight - scaledHeight) / 2;

    for (int charIndex = 0; text[charIndex] != '\0'; charIndex++) {
        const uint8_t* glyph = BitmapFont::GetGlyph(text[charIndex]);
        if (!glyph) {
            baseX += charWidth;
            continue;
        }

        for (int glyphY = 0; glyphY < BitmapFont::CHAR_HEIGHT; glyphY++) {
            for (int glyphX = 0; glyphX < BitmapFont::CHAR_WIDTH; glyphX++) {
                if (BitmapFont::IsPixelSet(glyph, glyphX, glyphY)) {
                    int pixelX = baseX + glyphX;
                    for (int scaleIndex = 0; scaleIndex < scaleY; scaleIndex++) {
                        int pixelY = baseY + yOffset + glyphY * scaleY + scaleIndex;
                        writePixel(pixelX, pixelY);
                    }
                }
            }
        }

        baseX += charWidth;
    }
}

} // namespace GlyphRenderer
