/**
 * ColorShader Implementation
 *
 * Draws solid colored rectangles using GX2 GPU.
 *
 * NOTE: This requires pre-compiled GX2 shader binaries.
 * The shader code would be:
 *
 * Vertex Shader (GLSL-style pseudo code):
 *   uniform vec2 uScreenSize;
 *   in vec2 aPosition;
 *   void main() {
 *       // Convert from screen coordinates to clip space
 *       vec2 pos = (aPosition / uScreenSize) * 2.0 - 1.0;
 *       gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);
 *   }
 *
 * Pixel Shader:
 *   uniform vec4 uColor;
 *   out vec4 fragColor;
 *   void main() {
 *       fragColor = uColor;
 *   }
 */

#ifdef ENABLE_GX2_RENDERING

#include "ColorShader.h"
#include <cstring>
#include <malloc.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <coreinit/cache.h>
#include <memory/mappedmemory.h>

namespace ColorShader {

// =============================================================================
// Shader State
// =============================================================================

static bool sInitialized = false;

// Shader programs
static GX2VertexShader* sVertexShader = nullptr;
static GX2PixelShader* sPixelShader = nullptr;

// Fetch shader (describes vertex layout)
static GX2FetchShader* sFetchShader = nullptr;
static void* sFetchShaderBuffer = nullptr;

// Vertex attribute buffer
static GX2AttribStream sAttribStream;

// Uniform locations
static int32_t sScreenSizeLocation = -1;
static int32_t sColorLocation = -1;

// Current screen size
static float sScreenWidth = 1280.0f;
static float sScreenHeight = 720.0f;

// Vertex buffer for quad
struct Vertex {
    float x, y;
};

static void* sVertexBuffer = nullptr;
static const int VERTEX_COUNT = 4;

// =============================================================================
// Shader Binary Data
// =============================================================================

// TODO: Include actual compiled shader binaries here
// These would be compiled from WHB/GX2 shader source using the GPU7 compiler

// Placeholder - shader initialization will fail until real binaries are added
static const uint8_t sVertexShaderData[] = { 0 };
static const uint8_t sPixelShaderData[] = { 0 };

// =============================================================================
// Implementation
// =============================================================================

bool Init() {
    if (sInitialized) {
        return true;
    }

    // For now, just mark as not initialized since we don't have real shader binaries
    // When real binaries are available:
    // 1. Allocate and copy vertex shader
    // 2. Allocate and copy pixel shader
    // 3. Create fetch shader
    // 4. Set up attribute streams
    // 5. Allocate vertex buffer

    // Allocate vertex buffer (4 vertices for a quad)
    sVertexBuffer = MEMAllocFromMappedMemoryForGX2Ex(
        sizeof(Vertex) * VERTEX_COUNT,
        GX2_VERTEX_BUFFER_ALIGNMENT
    );

    if (!sVertexBuffer) {
        return false;
    }

    // TODO: Initialize actual shaders when binaries are available
    // For now, this remains uninitialized
    // sInitialized = true;

    return false; // Return false until we have real shader binaries
}

void Shutdown() {
    if (!sInitialized) {
        // Still clean up vertex buffer if allocated
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

    // Free vertex/pixel shaders
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

    // Free vertex buffer
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

    // Enable blending for alpha
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

void DrawRect(float x, float y, float width, float height, uint32_t color) {
    if (!sInitialized || !sVertexBuffer) {
        return;
    }

    // Set up vertices for a quad (triangle strip)
    Vertex* vertices = (Vertex*)sVertexBuffer;
    vertices[0] = { x, y };                     // Top-left
    vertices[1] = { x + width, y };             // Top-right
    vertices[2] = { x, y + height };            // Bottom-left
    vertices[3] = { x + width, y + height };    // Bottom-right

    // Flush vertex buffer to GPU
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, sVertexBuffer, sizeof(Vertex) * VERTEX_COUNT);

    // Set color uniform (convert RGBA to float4)
    float colorFloat[4] = {
        ((color >> 24) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        (color & 0xFF) / 255.0f
    };
    GX2SetPixelUniformReg(sColorLocation, 4, colorFloat);

    // Set vertex buffer
    GX2SetAttribBuffer(0, sizeof(Vertex) * VERTEX_COUNT, sizeof(Vertex), sVertexBuffer);

    // Draw triangle strip
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, VERTEX_COUNT, 0, 1);
}

} // namespace ColorShader

#endif // ENABLE_GX2_RENDERING
