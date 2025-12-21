/**
 * Renderer stub for web preview
 * Provides the same interface as the main renderer
 */

#pragma once

#include <cstdint>

namespace Renderer {

// =============================================================================
// Backend Selection
// =============================================================================

enum class Backend {
    OS_SCREEN,
    GX2
};

void SetBackend(Backend backend);
Backend GetBackend();

// =============================================================================
// Image Handle
// =============================================================================

struct ImageData {
    uint32_t* pixels;
    int width;
    int height;
};

using ImageHandle = ImageData*;
constexpr ImageHandle INVALID_IMAGE = nullptr;

// =============================================================================
// Lifecycle
// =============================================================================

bool Init();
void Shutdown();
bool IsInitialized();

// =============================================================================
// Frame Management
// =============================================================================

void BeginFrame(uint32_t clearColor);
void EndFrame();

// =============================================================================
// Text Rendering
// =============================================================================

void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF);
void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...);
void DrawTextF(int col, int row, const char* fmt, ...);

// =============================================================================
// Image Rendering
// =============================================================================

bool SupportsImages();
void DrawImage(int x, int y, ImageHandle image, int width = 0, int height = 0);
void DrawPlaceholder(int x, int y, int width, int height, uint32_t color);

// =============================================================================
// Coordinate Conversion
// =============================================================================

int ColToPixelX(int col);
int RowToPixelY(int row);
int GetScreenWidth();
int GetScreenHeight();
int GetGridWidth();
int GetGridHeight();

// =============================================================================
// Dynamic Layout Helpers
// =============================================================================

int GetDividerCol();
int GetDetailsPanelCol();
int GetListWidth();
int GetVisibleRows();
int GetFooterRow();
int GetTitleNameWidth(bool showNumbers);

} // namespace Renderer
