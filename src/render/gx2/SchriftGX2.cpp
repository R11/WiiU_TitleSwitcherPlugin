/**
 * SchriftGX2 - TrueType Font Rendering for GX2
 *
 * Implementation using libschrift for glyph rasterization
 * and GX2 textures for GPU-accelerated rendering.
 */

#ifdef ENABLE_GX2_RENDERING

#include "SchriftGX2.h"
#include "shaders/Texture2DShader.h"
#include <cstring>
#include <malloc.h>
#include <coreinit/cache.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memory.h>
#include <gx2/mem.h>
#include <gx2/sampler.h>
#include <memory/mappedmemory.h>

#pragma GCC diagnostic ignored "-Wvolatile"
#include <glm/glm.hpp>

namespace SchriftGX2 {

// =============================================================================
// Font Implementation
// =============================================================================

Font::Font(const void* fontData, size_t fontDataSize, float pointSize)
    : mFont(nullptr)
    , mPointSize(pointSize)
    , mLineHeight(0)
    , mAscender(0)
    , mDescender(0)
{
    // Load font from memory
    mFont = sft_loadmem(fontData, fontDataSize);
    if (!mFont) {
        return;
    }

    // Configure SFT for rendering
    mSft.font = mFont;
    mSft.xScale = pointSize;
    mSft.yScale = pointSize;
    mSft.xOffset = 0;
    mSft.yOffset = 0;
    mSft.flags = SFT_DOWNWARD_Y;

    // Get line metrics
    SFT_LMetrics lmetrics;
    if (sft_lmetrics(&mSft, &lmetrics) == 0) {
        mAscender = (float)lmetrics.ascender;
        mDescender = (float)lmetrics.descender;
        mLineHeight = mAscender - mDescender + (float)lmetrics.lineGap;
    }
}

Font::~Font() {
    clearCache();
    if (mFont) {
        sft_freefont(mFont);
        mFont = nullptr;
    }
}

void Font::clearCache() {
    for (auto& pair : mGlyphCache) {
        if (pair.second.texture) {
            // Free texture surface data
            if (pair.second.texture->surface.image) {
                MEMFreeToMappedMemory(pair.second.texture->surface.image);
            }
            free(pair.second.texture);
        }
    }
    mGlyphCache.clear();
}

const GlyphData* Font::getGlyph(uint32_t codepoint) {
    // Check cache first
    auto it = mGlyphCache.find(codepoint);
    if (it != mGlyphCache.end()) {
        return &it->second;
    }

    // Render and cache the glyph
    if (renderGlyph(codepoint)) {
        return &mGlyphCache[codepoint];
    }

    return nullptr;
}

bool Font::renderGlyph(uint32_t codepoint) {
    // Look up glyph ID
    SFT_Glyph glyph;
    if (sft_lookup(&mSft, codepoint, &glyph) < 0) {
        return false;
    }

    // Get glyph metrics
    SFT_GMetrics metrics;
    if (sft_gmetrics(&mSft, glyph, &metrics) < 0) {
        return false;
    }

    // Create glyph data entry
    GlyphData glyphData = {};
    glyphData.width = metrics.minWidth;
    glyphData.height = metrics.minHeight;
    glyphData.offsetX = (int)metrics.leftSideBearing;
    glyphData.offsetY = metrics.yOffset;
    glyphData.advanceX = (int)metrics.advanceWidth;

    // Handle empty glyphs (like space)
    if (metrics.minWidth <= 0 || metrics.minHeight <= 0) {
        glyphData.texture = nullptr;
        mGlyphCache[codepoint] = glyphData;
        return true;
    }

    // Allocate render buffer (8-bit grayscale)
    size_t bufferSize = metrics.minWidth * metrics.minHeight;
    uint8_t* buffer = (uint8_t*)malloc(bufferSize);
    if (!buffer) {
        return false;
    }
    memset(buffer, 0, bufferSize);

    // Render glyph to buffer
    SFT_Image image;
    image.pixels = buffer;
    image.width = metrics.minWidth;
    image.height = metrics.minHeight;

    if (sft_render(&mSft, glyph, image) < 0) {
        free(buffer);
        return false;
    }

    // Create GX2 texture (R8 format for grayscale)
    GX2Texture* texture = (GX2Texture*)calloc(1, sizeof(GX2Texture));
    if (!texture) {
        free(buffer);
        return false;
    }

    texture->surface.width = metrics.minWidth;
    texture->surface.height = metrics.minHeight;
    texture->surface.depth = 1;
    texture->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    texture->surface.format = GX2_SURFACE_FORMAT_UNORM_R8;
    texture->surface.aa = GX2_AA_MODE1X;
    texture->surface.use = GX2_SURFACE_USE_TEXTURE;
    texture->surface.mipLevels = 1;
    texture->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture->viewNumMips = 1;
    texture->viewNumSlices = 1;
    texture->compMap = 0x00000000; // R channel only

    // Calculate surface info
    GX2CalcSurfaceSizeAndAlignment(&texture->surface);

    // Allocate texture memory
    texture->surface.image = MEMAllocFromMappedMemoryForGX2Ex(
        texture->surface.imageSize,
        texture->surface.alignment
    );

    if (!texture->surface.image) {
        free(texture);
        free(buffer);
        return false;
    }

    // Copy glyph data to texture
    // Need to handle pitch alignment
    uint32_t pitch = texture->surface.pitch;
    uint8_t* dst = (uint8_t*)texture->surface.image;

    for (int y = 0; y < metrics.minHeight; y++) {
        memcpy(dst + y * pitch, buffer + y * metrics.minWidth, metrics.minWidth);
    }

    // Flush cache
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture->surface.image, texture->surface.imageSize);

    // Initialize texture for sampling
    GX2InitTextureRegs(texture);

    // Clean up render buffer
    free(buffer);

    // Store in cache
    glyphData.texture = texture;
    mGlyphCache[codepoint] = glyphData;

    return true;
}

float Font::getStringWidth(const char* text) {
    float width = 0;

    // Simple UTF-8 decoding
    const uint8_t* p = (const uint8_t*)text;
    while (*p) {
        uint32_t codepoint;
        if ((*p & 0x80) == 0) {
            codepoint = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            codepoint = (*p++ & 0x1F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            codepoint = (*p++ & 0x0F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            codepoint = (*p++ & 0x07) << 18;
            codepoint |= (*p++ & 0x3F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else {
            // Invalid UTF-8, skip byte
            p++;
            continue;
        }

        const GlyphData* glyph = getGlyph(codepoint);
        if (glyph) {
            width += glyph->advanceX;
        }
    }

    return width;
}

// =============================================================================
// Module Functions
// =============================================================================

static bool sInitialized = false;
static GX2Sampler* sFontSampler = nullptr;
static float sScreenWidth = 854.0f;
static float sScreenHeight = 480.0f;

bool Init() {
    if (sInitialized) {
        return true;
    }

    sFontSampler = (GX2Sampler*)MEMAllocFromMappedMemoryForGX2Ex(sizeof(GX2Sampler), 64);
    if (!sFontSampler) {
        return false;
    }
    GX2InitSampler(sFontSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    sInitialized = true;
    return true;
}

void Shutdown() {
    if (!sInitialized) {
        return;
    }

    if (sFontSampler) {
        MEMFreeToMappedMemory(sFontSampler);
        sFontSampler = nullptr;
    }

    sInitialized = false;
}

void SetScreenSize(float width, float height) {
    sScreenWidth = width;
    sScreenHeight = height;
}

Font* LoadDefaultFont(float pointSize) {
    void* fontData = nullptr;
    uint32_t fontSize = 0;

    if (!OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &fontData, &fontSize)) {
        return nullptr;
    }
    if (!fontData || fontSize == 0) {
        return nullptr;
    }

    return new Font(fontData, fontSize, pointSize);
}

void DrawText(Font* font, float x, float y, const char* text, uint32_t color) {
    if (!font || !font->isValid() || !sFontSampler) {
        return;
    }

    float cursorX = x;
    float baseline = y + font->getAscender();

    // Convert RGBA color to vec4
    glm::vec4 colorVec(
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f
    );

    // Simple UTF-8 decoding
    const uint8_t* p = (const uint8_t*)text;
    while (*p) {
        uint32_t codepoint;
        if ((*p & 0x80) == 0) {
            codepoint = *p++;
        } else if ((*p & 0xE0) == 0xC0) {
            codepoint = (*p++ & 0x1F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF0) == 0xE0) {
            codepoint = (*p++ & 0x0F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else if ((*p & 0xF8) == 0xF0) {
            codepoint = (*p++ & 0x07) << 18;
            codepoint |= (*p++ & 0x3F) << 12;
            codepoint |= (*p++ & 0x3F) << 6;
            codepoint |= (*p++ & 0x3F);
        } else {
            p++;
            continue;
        }

        const GlyphData* glyph = font->getGlyph(codepoint);
        if (glyph) {
            if (glyph->texture) {
                float glyphX = cursorX + glyph->offsetX;
                float glyphY = baseline + glyph->offsetY;

                // Calculate quad center and size in NDC
                float centerX = glyphX + glyph->width / 2.0f;
                float centerY = glyphY + glyph->height / 2.0f;

                glm::vec3 offset(
                    (centerX / sScreenWidth) * 2.0f - 1.0f,
                    1.0f - (centerY / sScreenHeight) * 2.0f,
                    0.0f
                );

                glm::vec3 scale(
                    (float)glyph->width / sScreenWidth,
                    (float)glyph->height / sScreenHeight,
                    1.0f
                );

                Texture2DShader::instance()->setShaders();
                Texture2DShader::instance()->setAttributeBuffer();
                Texture2DShader::instance()->setAngle(0.0f);
                Texture2DShader::instance()->setOffset(offset);
                Texture2DShader::instance()->setScale(scale);
                Texture2DShader::instance()->setColorIntensity(colorVec);
                Texture2DShader::instance()->setBlurring(glm::vec3(0.0f));
                Texture2DShader::instance()->setTextureAndSampler(glyph->texture, sFontSampler);
                Texture2DShader::instance()->draw();
            }
            cursorX += glyph->advanceX;
        }
    }
}

} // namespace SchriftGX2

#endif // ENABLE_GX2_RENDERING
