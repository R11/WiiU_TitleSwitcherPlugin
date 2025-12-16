/**
 * Abstract Renderer Interface
 *
 * This interface abstracts the rendering backend, allowing different
 * implementations (OSScreen for text-only, GX2 for full graphics).
 *
 * DESIGN GOALS:
 * -------------
 * 1. Keep rendering logic separate from menu logic
 * 2. Allow swapping backends without changing menu code
 * 3. Support future features like image rendering
 *
 * IMPLEMENTATIONS:
 * ----------------
 * - RendererOSScreen: Simple text-based rendering (current)
 * - RendererGX2: Full GPU rendering with image support (future)
 *
 * USAGE:
 * ------
 *   // At startup, select a renderer
 *   Renderer::SetBackend(Renderer::Backend::OS_SCREEN);
 *
 *   // Use the renderer
 *   Renderer::Init();
 *   Renderer::BeginFrame(bgColor);
 *   Renderer::DrawText(0, 0, "Hello");
 *   Renderer::EndFrame();
 *   Renderer::Shutdown();
 */

#pragma once

#include <cstdint>

namespace Renderer {

// =============================================================================
// Backend Selection
// =============================================================================

/**
 * Available rendering backends.
 */
enum class Backend {
    OS_SCREEN,  // Text-only OSScreen (fast init, simple)
    GX2         // Full GPU rendering (images, effects)
};

/**
 * Set the active rendering backend.
 * Must be called before Init().
 *
 * @param backend The backend to use
 */
void SetBackend(Backend backend);

/**
 * Get the currently active backend.
 *
 * @return The active backend
 */
Backend GetBackend();

// =============================================================================
// Image Handle
// =============================================================================

/**
 * Opaque handle to a loaded image.
 * The actual data depends on the backend implementation.
 */
using ImageHandle = void*;

/**
 * Invalid/null image handle constant.
 */
constexpr ImageHandle INVALID_IMAGE = nullptr;

// =============================================================================
// Lifecycle
// =============================================================================

/**
 * Initialize the renderer.
 * Must be called before any drawing operations.
 *
 * @return true if initialization succeeded
 */
bool Init();

/**
 * Shut down the renderer and release resources.
 */
void Shutdown();

/**
 * Check if the renderer is initialized.
 *
 * @return true if Init() has been called successfully
 */
bool IsInitialized();

// =============================================================================
// Frame Management
// =============================================================================

/**
 * Begin a new frame.
 * Clears the screen and prepares for drawing.
 *
 * @param clearColor Background color (RGBA format: 0xRRGGBBAA)
 */
void BeginFrame(uint32_t clearColor);

/**
 * End the current frame.
 * Flushes drawing commands and presents to screen.
 */
void EndFrame();

// =============================================================================
// Text Rendering
// =============================================================================

/**
 * Draw text at a character grid position.
 *
 * @param col    Column (0-based, from left)
 * @param row    Row (0-based, from top)
 * @param text   Null-terminated string to draw
 * @param color  Text color (RGBA format), 0 = default white
 */
void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF);

/**
 * Draw formatted text at a character grid position.
 *
 * @param col    Column (0-based, from left)
 * @param row    Row (0-based, from top)
 * @param color  Text color (RGBA format)
 * @param fmt    Printf-style format string
 * @param ...    Format arguments
 */
void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...);

/**
 * Draw formatted text with default color.
 *
 * @param col    Column (0-based, from left)
 * @param row    Row (0-based, from top)
 * @param fmt    Printf-style format string
 * @param ...    Format arguments
 */
void DrawTextF(int col, int row, const char* fmt, ...);

// =============================================================================
// Image Rendering (Backend-dependent)
// =============================================================================

/**
 * Check if the current backend supports image rendering.
 *
 * @return true if DrawImage() will work
 */
bool SupportsImages();

/**
 * Draw an image at pixel coordinates.
 * Only works if SupportsImages() returns true.
 *
 * @param x      X position in pixels
 * @param y      Y position in pixels
 * @param image  Image handle from ImageLoader
 * @param width  Desired width (0 = original size)
 * @param height Desired height (0 = original size)
 */
void DrawImage(int x, int y, ImageHandle image, int width = 0, int height = 0);

/**
 * Draw a placeholder rectangle where an image would go.
 * Used while images are loading.
 *
 * @param x      X position in pixels
 * @param y      Y position in pixels
 * @param width  Width of placeholder
 * @param height Height of placeholder
 * @param color  Fill color (RGBA format)
 */
void DrawPlaceholder(int x, int y, int width, int height, uint32_t color);

// =============================================================================
// Coordinate Conversion
// =============================================================================

/**
 * Convert character column to pixel X coordinate.
 *
 * @param col Character column
 * @return Pixel X coordinate
 */
int ColToPixelX(int col);

/**
 * Convert character row to pixel Y coordinate.
 *
 * @param row Character row
 * @return Pixel Y coordinate
 */
int RowToPixelY(int row);

/**
 * Get the screen width in pixels.
 *
 * @return Screen width
 */
int GetScreenWidth();

/**
 * Get the screen height in pixels.
 *
 * @return Screen height
 */
int GetScreenHeight();

/**
 * Get the character grid width (columns).
 *
 * @return Number of text columns
 */
int GetGridWidth();

/**
 * Get the character grid height (rows).
 *
 * @return Number of text rows
 */
int GetGridHeight();

} // namespace Renderer
