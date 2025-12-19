/**
 * SchriftGX2 - TrueType Font Rendering for GX2
 *
 * Uses libschrift to rasterize glyphs and renders them as GX2 textures.
 * Based on the NotificationModule implementation.
 */

#pragma once

#include <cstdint>
#include <map>
#include <gx2/texture.h>
#include "schrift.h"

namespace SchriftGX2 {

/**
 * Cached glyph data for quick rendering.
 */
struct GlyphData {
    GX2Texture* texture;
    int width;
    int height;
    int offsetX;
    int offsetY;
    int advanceX;
};

/**
 * Font instance with glyph cache.
 */
class Font {
public:
    /**
     * Create a font from memory data.
     * @param fontData Pointer to TrueType font data
     * @param fontDataSize Size of font data in bytes
     * @param pointSize Font size in points
     */
    Font(const void* fontData, size_t fontDataSize, float pointSize);
    ~Font();

    /**
     * Check if font loaded successfully.
     */
    bool isValid() const { return mFont != nullptr; }

    /**
     * Get cached glyph data, rendering if necessary.
     * @param codepoint Unicode codepoint
     * @return Glyph data or nullptr if not available
     */
    const GlyphData* getGlyph(uint32_t codepoint);

    /**
     * Get line height for this font size.
     */
    float getLineHeight() const { return mLineHeight; }

    /**
     * Get ascender (distance from baseline to top).
     */
    float getAscender() const { return mAscender; }

    /**
     * Get descender (distance from baseline to bottom, typically negative).
     */
    float getDescender() const { return mDescender; }

    /**
     * Calculate the width of a string in pixels.
     * @param text UTF-8 encoded text
     * @return Width in pixels
     */
    float getStringWidth(const char* text);

private:
    SFT_Font* mFont;
    SFT mSft;
    float mPointSize;
    float mLineHeight;
    float mAscender;
    float mDescender;

    // Glyph cache: codepoint -> glyph data
    std::map<uint32_t, GlyphData> mGlyphCache;

    /**
     * Render a glyph to a GX2 texture.
     * @param codepoint Unicode codepoint
     * @return true on success
     */
    bool renderGlyph(uint32_t codepoint);

    /**
     * Free all cached glyph textures.
     */
    void clearCache();
};

/**
 * Initialize the SchriftGX2 system.
 * Must be called before using any fonts.
 * @return true on success
 */
bool Init();

/**
 * Shutdown the SchriftGX2 system.
 * Frees all resources.
 */
void Shutdown();

/**
 * Set the screen size for text positioning.
 * @param width Screen width in pixels
 * @param height Screen height in pixels
 */
void SetScreenSize(float width, float height);

/**
 * Load the default font from the embedded font data.
 * @param pointSize Font size in points
 * @return Font instance or nullptr on failure
 */
Font* LoadDefaultFont(float pointSize);

/**
 * Draw text at the specified position.
 * @param font Font to use
 * @param x X position in pixels
 * @param y Y position in pixels (baseline)
 * @param text UTF-8 encoded text
 * @param color RGBA color
 */
void DrawText(Font* font, float x, float y, const char* text, uint32_t color);

} // namespace SchriftGX2
