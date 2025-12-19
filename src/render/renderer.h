/**
 * Abstract Renderer Interface
 * Abstracts rendering backend (OSScreen text, GX2 graphics).
 */

#pragma once

#include <cstdint>

namespace Renderer {

enum class Backend {
    OS_SCREEN,
    GX2
};

void SetBackend(Backend backend);
Backend GetBackend();

struct ImageData {
    uint32_t* pixels;
    int width;
    int height;
};

using ImageHandle = ImageData*;
constexpr ImageHandle INVALID_IMAGE = nullptr;

bool Init();
void Shutdown();
bool IsInitialized();

void BeginFrame(uint32_t clearColor);
void EndFrame();

void DrawText(int column, int row, const char* text, uint32_t color = 0xFFFFFFFF);
void DrawTextF(int column, int row, uint32_t color, const char* format, ...);
void DrawTextF(int column, int row, const char* format, ...);

bool SupportsImages();
void DrawImage(int pixelX, int pixelY, ImageHandle image, int width = 0, int height = 0);
void DrawPlaceholder(int pixelX, int pixelY, int width, int height, uint32_t color);

int ColToPixelX(int column);
int RowToPixelY(int row);

int GetScreenWidth();
int GetScreenHeight();
int GetGridWidth();
int GetGridHeight();

int GetDividerCol();
int GetDetailsPanelCol();
int GetListWidth();
int GetVisibleRows();
int GetFooterRow();
int GetTitleNameWidth(bool showLineNumbers);

}
