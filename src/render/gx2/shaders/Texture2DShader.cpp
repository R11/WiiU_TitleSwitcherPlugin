/**
 * Texture2DShader Implementation
 *
 * Draws textured quads with color tint using GX2 GPU.
 *
 * NOTE: This requires pre-compiled GX2 shader binaries.
 * The shader code would be:
 *
 * Vertex Shader (GLSL-style pseudo code):
 *   uniform vec2 uScreenSize;
 *   in vec2 aPosition;
 *   in vec2 aTexCoord;
 *   out vec2 vTexCoord;
 *   void main() {
 *       vec2 pos = (aPosition / uScreenSize) * 2.0 - 1.0;
 *       gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);
 *       vTexCoord = aTexCoord;
 *   }
 *
 * Pixel Shader:
 *   uniform vec4 uColor;
 *   uniform sampler2D uTexture;
 *   in vec2 vTexCoord;
 *   out vec4 fragColor;
 *   void main() {
 *       vec4 texColor = texture(uTexture, vTexCoord);
 *       // For grayscale font textures, use red channel as alpha
 *       fragColor = vec4(uColor.rgb, uColor.a * texColor.r);
 *   }
 */

#ifdef ENABLE_GX2_RENDERING

#include "Texture2DShader.h"
#include <cstring>
#include <malloc.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <coreinit/cache.h>
#include <memory/mappedmemory.h>

namespace Texture2DShader {

// =============================================================================
// Shader State
// =============================================================================

static bool sInitialized = false;

// Shader programs
static GX2VertexShader* sVertexShader = nullptr;
static GX2PixelShader* sPixelShader = nullptr;

// Fetch shader
static GX2FetchShader* sFetchShader = nullptr;
static void* sFetchShaderBuffer = nullptr;

// Sampler for texture filtering
static GX2Sampler sSampler;

// Uniform locations
static int32_t sScreenSizeLocation = -1;
static int32_t sColorLocation = -1;
static int32_t sTextureLocation = -1;
static int32_t sSamplerLocation = -1;

// Current screen size
static float sScreenWidth = 1280.0f;
static float sScreenHeight = 720.0f;

// Vertex buffer
struct Vertex {
    float x, y;     // Position
    float u, v;     // Texture coordinates
};

static void* sVertexBuffer = nullptr;
static const int VERTEX_COUNT = 4;

// =============================================================================
// Implementation
// =============================================================================

bool Init() {
    if (sInitialized) {
        return true;
    }

    // Allocate vertex buffer
    sVertexBuffer = MEMAllocFromMappedMemoryForGX2Ex(
        sizeof(Vertex) * VERTEX_COUNT,
        GX2_VERTEX_BUFFER_ALIGNMENT
    );

    if (!sVertexBuffer) {
        return false;
    }

    // Initialize sampler for linear filtering
    GX2InitSampler(&sSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // TODO: Initialize actual shaders when binaries are available
    // For now, this remains uninitialized
    // sInitialized = true;

    return false; // Return false until we have real shader binaries
}

void Shutdown() {
    if (!sInitialized) {
        if (sVertexBuffer) {
            MEMFreeToMappedMemory(sVertexBuffer);
            sVertexBuffer = nullptr;
        }
        return;
    }

    // Free fetch shader
    if (sFetchShaderBuffer) {
        MEMFreeToMappedMemory(sFetchShaderBuffer);
        sFetchShaderBuffer = nullptr;
    }
    if (sFetchShader) {
        free(sFetchShader);
        sFetchShader = nullptr;
    }

    // Free shaders
    if (sVertexShader) {
        if (sVertexShader->program) {
            MEMFreeToMappedMemory((void*)sVertexShader->program);
        }
        free(sVertexShader);
        sVertexShader = nullptr;
    }

    if (sPixelShader) {
        if (sPixelShader->program) {
            MEMFreeToMappedMemory((void*)sPixelShader->program);
        }
        free(sPixelShader);
        sPixelShader = nullptr;
    }

    if (sVertexBuffer) {
        MEMFreeToMappedMemory(sVertexBuffer);
        sVertexBuffer = nullptr;
    }

    sInitialized = false;
}

bool IsInitialized() {
    return sInitialized;
}

void SetScreenSize(float width, float height) {
    sScreenWidth = width;
    sScreenHeight = height;
}

void Begin() {
    if (!sInitialized) {
        return;
    }

    // Set shaders
    GX2SetVertexShader(sVertexShader);
    GX2SetPixelShader(sPixelShader);
    GX2SetFetchShader(sFetchShader);

    // Set screen size uniform
    float screenSize[4] = { sScreenWidth, sScreenHeight, 0, 0 };
    GX2SetVertexUniformReg(sScreenSizeLocation, 4, screenSize);

    // Enable blending for text alpha
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
}

void End() {
    // Nothing to do
}

void DrawTexture(GX2Texture* texture, float x, float y, float width, float height, uint32_t color) {
    if (!texture) {
        return;
    }

    // Use texture dimensions if not specified
    if (width <= 0) {
        width = (float)texture->surface.width;
    }
    if (height <= 0) {
        height = (float)texture->surface.height;
    }

    DrawTextureUV(texture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void DrawTextureUV(GX2Texture* texture, float x, float y, float width, float height,
                   float u0, float v0, float u1, float v1, uint32_t color) {
    if (!sInitialized || !sVertexBuffer || !texture) {
        return;
    }

    // Set up vertices
    Vertex* vertices = (Vertex*)sVertexBuffer;
    vertices[0] = { x, y, u0, v0 };                     // Top-left
    vertices[1] = { x + width, y, u1, v0 };             // Top-right
    vertices[2] = { x, y + height, u0, v1 };            // Bottom-left
    vertices[3] = { x + width, y + height, u1, v1 };    // Bottom-right

    // Flush vertex buffer
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sVertexBuffer, sizeof(Vertex) * VERTEX_COUNT);

    // Set color uniform
    float colorFloat[4] = {
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f
    };
    GX2SetPixelUniformReg(sColorLocation, 4, colorFloat);

    // Bind texture and sampler
    GX2SetPixelTexture(texture, sTextureLocation);
    GX2SetPixelSampler(&sSampler, sSamplerLocation);

    // Set vertex buffer
    GX2SetAttribBuffer(0, sizeof(Vertex) * VERTEX_COUNT, sizeof(Vertex), sVertexBuffer);

    // Draw
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, VERTEX_COUNT, 0, 1);
}

} // namespace Texture2DShader

#endif // ENABLE_GX2_RENDERING
