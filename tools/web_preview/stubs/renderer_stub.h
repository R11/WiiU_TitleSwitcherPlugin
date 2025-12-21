/**
 * Renderer Stub for Web Preview
 *
 * Declares the Renderer interface. Implementation is in canvas_renderer.cpp.
 * This file overrides the preview tool's renderer_stub.h when included.
 */

#pragma once

#include <cstdint>
#include "layout_stub.h"

namespace Renderer {

// =============================================================================
// Screen Configuration
// =============================================================================

struct ScreenConfig {
    const char* name;
    int pixelWidth;
    int pixelHeight;
    int gridCols;
    int gridRows;
    int charWidth;
    int charHeight;
    bool is4x3;
};

enum class Backend {
    OS_SCREEN,
    GX2,
    PREVIEW_ASCII,
    CANVAS
};

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
// Screen Selection
// =============================================================================

void SelectScreen(int screen);
void SetTvResolution(int resolution);

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
// Pixel Drawing (for debug overlays)
// =============================================================================

void DrawPixel(int x, int y, uint32_t color);
void DrawHLine(int x, int y, int length, uint32_t color);
void DrawVLine(int x, int y, int length, uint32_t color);

// =============================================================================
// Coordinate Helpers
// =============================================================================

int ColToPixelX(int col);
int RowToPixelY(int row);
int GetScreenWidth();
int GetScreenHeight();
int GetGridWidth();
int GetGridHeight();

// =============================================================================
// Layout Helpers
// =============================================================================

int GetDividerCol();
int GetDetailsPanelCol();
int GetListWidth();
int GetVisibleRows();
int GetFooterRow();
int GetTitleNameWidth(bool showNumbers);

// =============================================================================
// Additional Helpers (for menu_render.cpp compatibility)
// =============================================================================

const ScreenConfig& GetScreenConfig();
int GetIconSize();
int GetIconX();
int GetIconY();

// =============================================================================
// Backend Selection
// =============================================================================

void SetBackend(Backend backend);
Backend GetBackend();

// =============================================================================
// Layout System Access
// =============================================================================

const Layout::PixelLayout& GetLayout();
void SetLayoutPreferences(const Layout::LayoutPreferences& prefs);

} // namespace Renderer
