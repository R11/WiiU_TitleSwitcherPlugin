/**
 * Pixel Editor Implementation
 *
 * Uses libgd for canvas operations, OSScreen for display.
 */

#include "pixel_editor.h"
#include "../render/renderer.h"
#include "../input/buttons.h"

#include <gd.h>
#include <vpad/input.h>
#include <coreinit/screen.h>
#include <coreinit/filesystem.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Helper to ensure directory exists
static void ensureDirectoryExists(const char* path)
{
    // Try to create the directory (will fail silently if it exists)
    FSClient* client = (FSClient*)malloc(sizeof(FSClient));
    FSCmdBlock* block = (FSCmdBlock*)malloc(sizeof(FSCmdBlock));

    if (client && block) {
        FSAddClient(client, FS_ERROR_FLAG_NONE);
        FSInitCmdBlock(block);
        FSMakeDir(client, block, path, FS_ERROR_FLAG_NONE);
        FSDelClient(client, FS_ERROR_FLAG_NONE);
    }

    if (client) free(client);
    if (block) free(block);
}

namespace PixelEditor {

// =============================================================================
// Built-in Palettes
// =============================================================================

const Palette PALETTE_DEFAULT = {
    {
        0x000000FF,  // Black
        0xFFFFFFFF,  // White
        0xFF0000FF,  // Red
        0x00FF00FF,  // Green
        0x0000FFFF,  // Blue
        0xFFFF00FF,  // Yellow
        0xFF00FFFF,  // Magenta
        0x00FFFFFF,  // Cyan
        0x808080FF,  // Gray
        0x800000FF,  // Dark Red
        0x008000FF,  // Dark Green
        0x000080FF,  // Dark Blue
        0x808000FF,  // Olive
        0x800080FF,  // Purple
        0x008080FF,  // Teal
        0xC0C0C0FF,  // Silver
    },
    16,
    "Default"
};

const Palette PALETTE_GAMEBOY = {
    {
        0x0F380FFF,  // Darkest
        0x306230FF,  // Dark
        0x8BAC0FFF,  // Light
        0x9BBC0FFF,  // Lightest
    },
    4,
    "GameBoy"
};

const Palette PALETTE_NES = {
    {
        0x000000FF, 0xFCFCFCFF, 0xF8F8F8FF, 0xBCBCBCFF,
        0x7C7C7CFF, 0xA4E4FCFF, 0x3CBCFCFF, 0x0078F8FF,
        0x0000FCFF, 0xB8B8F8FF, 0x6888FCFF, 0x0058F8FF,
        0x0000BCFF, 0xD8B8F8FF, 0x9878F8FF, 0x6844FCFF,
    },
    16,
    "NES"
};

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Editor state
bool sIsOpen = false;
gdImagePtr sCanvas = nullptr;
Config sConfig;

// View state
int sZoom = 4;           // Display zoom (1, 2, 4, 8)
int sViewX = 0;          // View offset X (in canvas pixels)
int sViewY = 0;          // View offset Y
int sCursorX = 0;        // Cursor position (in canvas pixels)
int sCursorY = 0;

// Tool state
Tool sCurrentTool = Tool::PENCIL;
int sCurrentColor = 0;   // Index into palette
const Palette* sPalette = &PALETTE_DEFAULT;
bool sShowGrid = true;

// Screen layout constants
constexpr int CANVAS_SCREEN_X = 50;    // Where canvas is drawn on screen
constexpr int CANVAS_SCREEN_Y = 30;
constexpr int CANVAS_SCREEN_W = 512;   // Max canvas display size
constexpr int CANVAS_SCREEN_H = 384;

constexpr int PALETTE_X = 600;
constexpr int PALETTE_Y = 50;
constexpr int PALETTE_CELL = 24;

constexpr int MINIMAP_X = 600;
constexpr int MINIMAP_Y = 250;
constexpr int MINIMAP_SIZE = 128;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Convert palette color (0xRRGGBBAA) to gd color index.
 */
int gdColorFromRGBA(gdImagePtr img, uint32_t rgba)
{
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    int a = 127 - ((rgba & 0xFF) >> 1);  // gd alpha: 0=opaque, 127=transparent
    return gdImageColorAllocateAlpha(img, r, g, b, a);
}

/**
 * Get RGBA from gd pixel.
 */
uint32_t rgbaFromGdPixel(gdImagePtr img, int x, int y)
{
    int pixel = gdImageGetPixel(img, x, y);
    int r = gdImageRed(img, pixel);
    int g = gdImageGreen(img, pixel);
    int b = gdImageBlue(img, pixel);
    int a = 255 - (gdImageAlpha(img, pixel) * 2);
    return (r << 24) | (g << 16) | (b << 8) | a;
}

/**
 * Clamp cursor to canvas bounds.
 */
void clampCursor()
{
    if (sCursorX < 0) sCursorX = 0;
    if (sCursorY < 0) sCursorY = 0;
    if (sCursorX >= sConfig.width) sCursorX = sConfig.width - 1;
    if (sCursorY >= sConfig.height) sCursorY = sConfig.height - 1;
}

/**
 * Update view to keep cursor visible.
 */
void updateView()
{
    int viewW = CANVAS_SCREEN_W / sZoom;
    int viewH = CANVAS_SCREEN_H / sZoom;

    // Keep cursor in view with margin
    int margin = 2;
    if (sCursorX < sViewX + margin) {
        sViewX = sCursorX - margin;
    }
    if (sCursorX >= sViewX + viewW - margin) {
        sViewX = sCursorX - viewW + margin + 1;
    }
    if (sCursorY < sViewY + margin) {
        sViewY = sCursorY - margin;
    }
    if (sCursorY >= sViewY + viewH - margin) {
        sViewY = sCursorY - viewH + margin + 1;
    }

    // Clamp view
    if (sViewX < 0) sViewX = 0;
    if (sViewY < 0) sViewY = 0;
    if (sViewX > sConfig.width - viewW) sViewX = sConfig.width - viewW;
    if (sViewY > sConfig.height - viewH) sViewY = sConfig.height - viewH;
    if (sViewX < 0) sViewX = 0;
    if (sViewY < 0) sViewY = 0;
}

/**
 * Flood fill algorithm.
 */
void floodFill(int x, int y, int targetColor, int fillColor)
{
    if (x < 0 || x >= sConfig.width || y < 0 || y >= sConfig.height) return;
    if (targetColor == fillColor) return;

    int current = gdImageGetPixel(sCanvas, x, y);
    if (current != targetColor) return;

    gdImageSetPixel(sCanvas, x, y, fillColor);

    // Recursive fill (could use stack for large canvases)
    floodFill(x + 1, y, targetColor, fillColor);
    floodFill(x - 1, y, targetColor, fillColor);
    floodFill(x, y + 1, targetColor, fillColor);
    floodFill(x, y - 1, targetColor, fillColor);
}

// =============================================================================
// Rendering
// =============================================================================

/**
 * Draw the canvas to screen.
 */
void drawCanvas()
{
    int viewW = CANVAS_SCREEN_W / sZoom;
    int viewH = CANVAS_SCREEN_H / sZoom;

    // Draw each visible pixel
    for (int cy = 0; cy < viewH && (sViewY + cy) < sConfig.height; cy++) {
        for (int cx = 0; cx < viewW && (sViewX + cx) < sConfig.width; cx++) {
            int canvasX = sViewX + cx;
            int canvasY = sViewY + cy;

            uint32_t color = rgbaFromGdPixel(sCanvas, canvasX, canvasY);

            int screenX = CANVAS_SCREEN_X + cx * sZoom;
            int screenY = CANVAS_SCREEN_Y + cy * sZoom;

            // Draw zoomed pixel
            for (int py = 0; py < sZoom; py++) {
                for (int px = 0; px < sZoom; px++) {
                    uint32_t rgbx = color & 0xFFFFFF00;
                    OSScreenPutPixelEx(SCREEN_TV, screenX + px, screenY + py, rgbx);
                    OSScreenPutPixelEx(SCREEN_DRC, screenX + px, screenY + py, rgbx);
                }
            }
        }
    }

    // Draw grid if enabled
    if (sShowGrid && sZoom >= 4) {
        uint32_t gridColor = 0x40404000;
        for (int cy = 0; cy <= viewH; cy++) {
            int screenY = CANVAS_SCREEN_Y + cy * sZoom;
            for (int sx = CANVAS_SCREEN_X; sx < CANVAS_SCREEN_X + viewW * sZoom; sx++) {
                OSScreenPutPixelEx(SCREEN_TV, sx, screenY, gridColor);
                OSScreenPutPixelEx(SCREEN_DRC, sx, screenY, gridColor);
            }
        }
        for (int cx = 0; cx <= viewW; cx++) {
            int screenX = CANVAS_SCREEN_X + cx * sZoom;
            for (int sy = CANVAS_SCREEN_Y; sy < CANVAS_SCREEN_Y + viewH * sZoom; sy++) {
                OSScreenPutPixelEx(SCREEN_TV, screenX, sy, gridColor);
                OSScreenPutPixelEx(SCREEN_DRC, screenX, sy, gridColor);
            }
        }
    }

    // Draw cursor
    int cursorScreenX = CANVAS_SCREEN_X + (sCursorX - sViewX) * sZoom;
    int cursorScreenY = CANVAS_SCREEN_Y + (sCursorY - sViewY) * sZoom;
    uint32_t cursorColor = 0xFF000000;  // Red outline

    // Draw cursor outline
    for (int i = 0; i < sZoom; i++) {
        OSScreenPutPixelEx(SCREEN_TV, cursorScreenX + i, cursorScreenY, cursorColor);
        OSScreenPutPixelEx(SCREEN_DRC, cursorScreenX + i, cursorScreenY, cursorColor);
        OSScreenPutPixelEx(SCREEN_TV, cursorScreenX + i, cursorScreenY + sZoom - 1, cursorColor);
        OSScreenPutPixelEx(SCREEN_DRC, cursorScreenX + i, cursorScreenY + sZoom - 1, cursorColor);
        OSScreenPutPixelEx(SCREEN_TV, cursorScreenX, cursorScreenY + i, cursorColor);
        OSScreenPutPixelEx(SCREEN_DRC, cursorScreenX, cursorScreenY + i, cursorColor);
        OSScreenPutPixelEx(SCREEN_TV, cursorScreenX + sZoom - 1, cursorScreenY + i, cursorColor);
        OSScreenPutPixelEx(SCREEN_DRC, cursorScreenX + sZoom - 1, cursorScreenY + i, cursorColor);
    }
}

/**
 * Draw the color palette.
 */
void drawPalette()
{
    Renderer::DrawText(PALETTE_X / 8, PALETTE_Y / 24 - 1, "Colors:");

    for (int i = 0; i < sPalette->count; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = PALETTE_X + col * (PALETTE_CELL + 2);
        int y = PALETTE_Y + row * (PALETTE_CELL + 2);

        uint32_t color = sPalette->colors[i];
        uint32_t rgbx = color & 0xFFFFFF00;

        // Draw color cell
        for (int py = 0; py < PALETTE_CELL; py++) {
            for (int px = 0; px < PALETTE_CELL; px++) {
                OSScreenPutPixelEx(SCREEN_TV, x + px, y + py, rgbx);
                OSScreenPutPixelEx(SCREEN_DRC, x + px, y + py, rgbx);
            }
        }

        // Highlight selected color
        if (i == sCurrentColor) {
            uint32_t highlight = 0xFFFFFF00;
            for (int j = 0; j < PALETTE_CELL; j++) {
                OSScreenPutPixelEx(SCREEN_TV, x + j, y, highlight);
                OSScreenPutPixelEx(SCREEN_DRC, x + j, y, highlight);
                OSScreenPutPixelEx(SCREEN_TV, x + j, y + PALETTE_CELL - 1, highlight);
                OSScreenPutPixelEx(SCREEN_DRC, x + j, y + PALETTE_CELL - 1, highlight);
                OSScreenPutPixelEx(SCREEN_TV, x, y + j, highlight);
                OSScreenPutPixelEx(SCREEN_DRC, x, y + j, highlight);
                OSScreenPutPixelEx(SCREEN_TV, x + PALETTE_CELL - 1, y + j, highlight);
                OSScreenPutPixelEx(SCREEN_DRC, x + PALETTE_CELL - 1, y + j, highlight);
            }
        }
    }
}

/**
 * Draw the minimap.
 */
void drawMinimap()
{
    Renderer::DrawText(MINIMAP_X / 8, MINIMAP_Y / 24 - 1, "Preview:");

    // Scale canvas to minimap size
    float scaleX = (float)MINIMAP_SIZE / sConfig.width;
    float scaleY = (float)MINIMAP_SIZE / sConfig.height;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    int drawW = (int)(sConfig.width * scale);
    int drawH = (int)(sConfig.height * scale);

    for (int my = 0; my < drawH; my++) {
        for (int mx = 0; mx < drawW; mx++) {
            int canvasX = (int)(mx / scale);
            int canvasY = (int)(my / scale);

            if (canvasX >= sConfig.width) canvasX = sConfig.width - 1;
            if (canvasY >= sConfig.height) canvasY = sConfig.height - 1;

            uint32_t color = rgbaFromGdPixel(sCanvas, canvasX, canvasY);
            uint32_t rgbx = color & 0xFFFFFF00;

            OSScreenPutPixelEx(SCREEN_TV, MINIMAP_X + mx, MINIMAP_Y + my, rgbx);
            OSScreenPutPixelEx(SCREEN_DRC, MINIMAP_X + mx, MINIMAP_Y + my, rgbx);
        }
    }

    // Draw viewport rectangle
    int viewW = CANVAS_SCREEN_W / sZoom;
    int viewH = CANVAS_SCREEN_H / sZoom;
    int rectX = MINIMAP_X + (int)(sViewX * scale);
    int rectY = MINIMAP_Y + (int)(sViewY * scale);
    int rectW = (int)(viewW * scale);
    int rectH = (int)(viewH * scale);

    uint32_t rectColor = 0xFF000000;
    for (int i = 0; i < rectW; i++) {
        OSScreenPutPixelEx(SCREEN_TV, rectX + i, rectY, rectColor);
        OSScreenPutPixelEx(SCREEN_DRC, rectX + i, rectY, rectColor);
        OSScreenPutPixelEx(SCREEN_TV, rectX + i, rectY + rectH, rectColor);
        OSScreenPutPixelEx(SCREEN_DRC, rectX + i, rectY + rectH, rectColor);
    }
    for (int i = 0; i < rectH; i++) {
        OSScreenPutPixelEx(SCREEN_TV, rectX, rectY + i, rectColor);
        OSScreenPutPixelEx(SCREEN_DRC, rectX, rectY + i, rectColor);
        OSScreenPutPixelEx(SCREEN_TV, rectX + rectW, rectY + i, rectColor);
        OSScreenPutPixelEx(SCREEN_DRC, rectX + rectW, rectY + i, rectColor);
    }
}

/**
 * Draw the toolbar/status.
 */
void drawToolbar()
{
    // Tool indicator
    const char* toolName = "Pencil";
    switch (sCurrentTool) {
        case Tool::PENCIL: toolName = "Pencil"; break;
        case Tool::ERASER: toolName = "Eraser"; break;
        case Tool::FILL: toolName = "Fill"; break;
        case Tool::COLOR_PICKER: toolName = "Picker"; break;
    }

    Renderer::DrawTextF(0, 0, "Tool: %s  Zoom: %dx  Grid: %s  Pos: %d,%d",
        toolName, sZoom, sShowGrid ? "ON" : "OFF", sCursorX, sCursorY);

    Renderer::DrawText(0, 17, "A:Draw  X:Eraser  Y:Fill  L/R:Zoom  +/-:Grid  B:Save&Exit");
}

/**
 * Render the full editor.
 */
void render()
{
    drawCanvas();
    drawPalette();
    drawMinimap();
    drawToolbar();
}

// =============================================================================
// Input Handling
// =============================================================================

/**
 * Handle touch screen input.
 * Returns true if touch was processed.
 */
bool handleTouch(const VPADStatus& vpad)
{
    // Get calibrated touch point
    VPADTouchData calibrated;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &calibrated, &vpad.tpNormal);

    if (!calibrated.touched) {
        return false;
    }

    // Calibrated touch coordinates are in DRC screen space (854x480)
    // But they might be scaled differently, so we handle both cases
    int screenX = calibrated.x;
    int screenY = calibrated.y;

    // If coordinates seem to be in larger range, scale them down
    if (screenX > 854 || screenY > 480) {
        screenX = (calibrated.x * 854) / 1280;
        screenY = (calibrated.y * 480) / 720;
    }

    // Check if within canvas display bounds
    int viewW = CANVAS_SCREEN_W / sZoom;
    int viewH = CANVAS_SCREEN_H / sZoom;
    int canvasEndX = CANVAS_SCREEN_X + viewW * sZoom;
    int canvasEndY = CANVAS_SCREEN_Y + viewH * sZoom;

    if (screenX >= CANVAS_SCREEN_X && screenX < canvasEndX &&
        screenY >= CANVAS_SCREEN_Y && screenY < canvasEndY) {
        // Convert to canvas coordinates
        int canvasX = sViewX + (screenX - CANVAS_SCREEN_X) / sZoom;
        int canvasY = sViewY + (screenY - CANVAS_SCREEN_Y) / sZoom;

        // Clamp to canvas bounds
        if (canvasX >= 0 && canvasX < sConfig.width &&
            canvasY >= 0 && canvasY < sConfig.height) {

            // Update cursor position
            sCursorX = canvasX;
            sCursorY = canvasY;

            // Draw at touch position
            int gdColor = gdColorFromRGBA(sCanvas, sPalette->colors[sCurrentColor]);

            switch (sCurrentTool) {
                case Tool::PENCIL:
                    gdImageSetPixel(sCanvas, canvasX, canvasY, gdColor);
                    break;
                case Tool::ERASER:
                    gdColor = gdColorFromRGBA(sCanvas, 0xFFFFFFFF);
                    gdImageSetPixel(sCanvas, canvasX, canvasY, gdColor);
                    break;
                case Tool::FILL:
                    // Fill only on initial touch (handled separately)
                    break;
                case Tool::COLOR_PICKER:
                    break;
            }
            return true;
        }
    }

    // Check if touch is in palette area
    if (screenX >= PALETTE_X && screenX < PALETTE_X + 4 * (PALETTE_CELL + 2) &&
        screenY >= PALETTE_Y && screenY < PALETTE_Y + 4 * (PALETTE_CELL + 2)) {
        int col = (screenX - PALETTE_X) / (PALETTE_CELL + 2);
        int row = (screenY - PALETTE_Y) / (PALETTE_CELL + 2);
        int colorIndex = row * 4 + col;
        if (colorIndex >= 0 && colorIndex < sPalette->count) {
            sCurrentColor = colorIndex;
            return true;
        }
    }

    return false;
}

/**
 * Handle input and return false to exit.
 */
bool handleInput(const VPADStatus& vpad)
{
    uint32_t pressed = vpad.trigger;
    uint32_t held = vpad.hold;

    // Handle touch input
    static bool wasTouching = false;

    // Get calibrated touch to check if touching
    VPADTouchData calibrated;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &calibrated, &vpad.tpNormal);
    bool isTouching = calibrated.touched != 0;

    if (isTouching) {
        handleTouch(vpad);

        // Fill on initial touch
        if (!wasTouching && sCurrentTool == Tool::FILL) {
            int gdColor = gdColorFromRGBA(sCanvas, sPalette->colors[sCurrentColor]);
            int target = gdImageGetPixel(sCanvas, sCursorX, sCursorY);
            floodFill(sCursorX, sCursorY, target, gdColor);
        }
    }
    wasTouching = isTouching;

    // Movement (with hold repeat)
    static int holdCounter = 0;
    bool moved = false;

    if (held & VPAD_BUTTON_UP) {
        if ((pressed & VPAD_BUTTON_UP) || holdCounter > 15) {
            sCursorY--;
            moved = true;
        }
    }
    if (held & VPAD_BUTTON_DOWN) {
        if ((pressed & VPAD_BUTTON_DOWN) || holdCounter > 15) {
            sCursorY++;
            moved = true;
        }
    }
    if (held & VPAD_BUTTON_LEFT) {
        if ((pressed & VPAD_BUTTON_LEFT) || holdCounter > 15) {
            sCursorX--;
            moved = true;
        }
    }
    if (held & VPAD_BUTTON_RIGHT) {
        if ((pressed & VPAD_BUTTON_RIGHT) || holdCounter > 15) {
            sCursorX++;
            moved = true;
        }
    }

    if (held & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_LEFT | VPAD_BUTTON_RIGHT)) {
        holdCounter++;
    } else {
        holdCounter = 0;
    }

    if (moved) {
        clampCursor();
        updateView();
        if (holdCounter > 15) holdCounter = 12;  // Faster repeat
    }

    // Draw with A (or held)
    if ((pressed & VPAD_BUTTON_A) || (held & VPAD_BUTTON_A)) {
        int gdColor = gdColorFromRGBA(sCanvas, sPalette->colors[sCurrentColor]);

        switch (sCurrentTool) {
            case Tool::PENCIL:
                gdImageSetPixel(sCanvas, sCursorX, sCursorY, gdColor);
                break;
            case Tool::ERASER:
                gdColor = gdColorFromRGBA(sCanvas, 0xFFFFFFFF);
                gdImageSetPixel(sCanvas, sCursorX, sCursorY, gdColor);
                break;
            case Tool::FILL:
                if (pressed & VPAD_BUTTON_A) {
                    int target = gdImageGetPixel(sCanvas, sCursorX, sCursorY);
                    floodFill(sCursorX, sCursorY, target, gdColor);
                }
                break;
            case Tool::COLOR_PICKER:
                // Pick color from canvas (would need to find palette match)
                break;
        }
    }

    // Tool selection
    if (pressed & VPAD_BUTTON_X) {
        sCurrentTool = (sCurrentTool == Tool::ERASER) ? Tool::PENCIL : Tool::ERASER;
    }
    if (pressed & VPAD_BUTTON_Y) {
        sCurrentTool = (sCurrentTool == Tool::FILL) ? Tool::PENCIL : Tool::FILL;
    }

    // Zoom
    if (pressed & VPAD_BUTTON_R) {
        if (sZoom < 8) sZoom *= 2;
        updateView();
    }
    if (pressed & VPAD_BUTTON_L) {
        if (sZoom > 1) sZoom /= 2;
        updateView();
    }

    // Grid toggle
    if (pressed & VPAD_BUTTON_PLUS) {
        sShowGrid = !sShowGrid;
    }

    // Color selection with stick
    if (pressed & VPAD_STICK_L_EMULATION_LEFT) {
        sCurrentColor = (sCurrentColor - 1 + sPalette->count) % sPalette->count;
    }
    if (pressed & VPAD_STICK_L_EMULATION_RIGHT) {
        sCurrentColor = (sCurrentColor + 1) % sPalette->count;
    }

    // Save and exit
    if (pressed & VPAD_BUTTON_B) {
        return false;  // Exit
    }

    return true;  // Continue
}

// =============================================================================
// Save/Load
// =============================================================================

bool saveCanvas(const char* path)
{
    FILE* out = fopen(path, "wb");
    if (!out) return false;

    gdImagePng(sCanvas, out);
    fclose(out);
    return true;
}

bool loadCanvas(const char* path)
{
    FILE* in = fopen(path, "rb");
    if (!in) return false;

    gdImagePtr loaded = gdImageCreateFromPng(in);
    fclose(in);

    if (!loaded) return false;

    // Copy to our canvas
    int w = gdImageSX(loaded);
    int h = gdImageSY(loaded);

    for (int y = 0; y < h && y < sConfig.height; y++) {
        for (int x = 0; x < w && x < sConfig.width; x++) {
            int pixel = gdImageGetPixel(loaded, x, y);
            gdImageSetPixel(sCanvas, x, y, pixel);
        }
    }

    gdImageDestroy(loaded);
    return true;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

bool Open(const Config& config)
{
    if (sIsOpen) return false;

    sConfig = config;
    sIsOpen = true;

    // Create canvas
    sCanvas = gdImageCreateTrueColor(config.width, config.height);
    if (!sCanvas) {
        sIsOpen = false;
        return false;
    }

    // Try to load previous drawing first
    char autoLoadPath[256];
    snprintf(autoLoadPath, sizeof(autoLoadPath), "%sdrawing.png", config.savePath);

    bool loaded = false;
    if (config.loadFile) {
        // Load specified file
        loaded = loadCanvas(config.loadFile);
    } else {
        // Try to load auto-saved drawing
        loaded = loadCanvas(autoLoadPath);
    }

    // Fill with white only if no image was loaded
    if (!loaded) {
        int white = gdImageColorAllocate(sCanvas, 255, 255, 255);
        gdImageFilledRectangle(sCanvas, 0, 0, config.width - 1, config.height - 1, white);
    }

    // Reset state
    sZoom = 4;
    sViewX = 0;
    sViewY = 0;
    sCursorX = config.width / 2;
    sCursorY = config.height / 2;
    sCurrentTool = Tool::PENCIL;
    sCurrentColor = 0;
    sShowGrid = true;
    sPalette = &PALETTE_DEFAULT;

    updateView();

    // Main loop
    VPADStatus vpadStatus;
    VPADReadError vpadError;
    bool running = true;
    bool saved = false;

    while (running) {
        Renderer::BeginFrame(0x202020FF);

        render();

        Renderer::EndFrame();

        // Input
        int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);
        if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
            running = handleInput(vpadStatus);
        }
    }

    // Save on exit - ensure directory exists first
    ensureDirectoryExists(config.savePath);

    char savePath[256];
    snprintf(savePath, sizeof(savePath), "%sdrawing.png", config.savePath);
    saved = saveCanvas(savePath);

    // Cleanup
    gdImageDestroy(sCanvas);
    sCanvas = nullptr;
    sIsOpen = false;

    return saved;
}

bool Edit(const char* filePath)
{
    Config config;
    config.loadFile = filePath;
    config.width = 64;   // Will be overwritten by loaded file
    config.height = 64;
    return Open(config);
}

uint32_t* GetPixels()
{
    // Would need to convert gdImage to raw pixels
    return nullptr;
}

int GetWidth()
{
    return sConfig.width;
}

int GetHeight()
{
    return sConfig.height;
}

} // namespace PixelEditor
