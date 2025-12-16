/**
 * Screen Rendering Wrapper
 *
 * This module provides a clean interface for rendering text to the TV and GamePad
 * screens using the OSScreen API. It handles all the complexity of:
 *
 * - Saving/restoring display controller state (so games render correctly after)
 * - Allocating GX2-compatible framebuffers
 * - Proper initialization sequence (double clear+flip)
 * - Frame synchronization (vsync)
 * - Drawing text and simple shapes
 *
 * USAGE PATTERN:
 * --------------
 *   if (Screen::Init()) {
 *       while (menuOpen) {
 *           Screen::BeginFrame(backgroundColor);
 *           Screen::DrawText(0, 0, "Hello World");
 *           Screen::EndFrame();
 *       }
 *       Screen::Shutdown();
 *   }
 *
 * COORDINATE SYSTEM:
 * ------------------
 * OSScreen uses a character-based coordinate system, not pixels:
 *
 *   - TV:     Approximately 60 columns x 30 rows (depends on resolution)
 *   - GamePad: Approximately 60 columns x 18 rows
 *
 * Column 0 is the left edge, Row 0 is the top edge.
 * Each "cell" is 8x16 pixels in the default font.
 *
 * ATTRIBUTION:
 * ------------
 * The initialization pattern (DC register save/restore, double clear+flip,
 * MEMAllocFromMappedMemoryForGX2Ex) is derived from:
 *   - WiiUPluginLoaderBackend (GPLv3) by Maschell
 *   - https://github.com/wiiu-env/WiiUPluginLoaderBackend
 *
 * REFERENCE:
 * ----------
 * - OSScreen API: https://wut.devkitpro.org/group__coreinit__screen.html
 * - GX2 Events: https://wut.devkitpro.org/group__gx2__event.html
 */

#pragma once

#include <cstdint>

namespace Screen {

// =============================================================================
// Screen Dimensions
// =============================================================================
// Approximate character dimensions for each screen.
// Actual dimensions may vary slightly based on TV resolution settings.
// =============================================================================

// TV screen (main display)
constexpr int TV_COLS = 60;   // Characters per row
constexpr int TV_ROWS = 30;   // Rows of text

// DRC (GamePad) screen
constexpr int DRC_COLS = 60;  // Characters per row
constexpr int DRC_ROWS = 18;  // Rows of text

// =============================================================================
// Screen State
// =============================================================================

/**
 * Check if the screen system is currently initialized.
 *
 * @return true if Init() was called successfully and Shutdown() hasn't been called
 */
bool IsInitialized();

// =============================================================================
// Lifecycle Functions
// =============================================================================

/**
 * Initialize the screen rendering system.
 *
 * This function:
 * 1. Saves the current display controller state (for later restoration)
 * 2. Initializes OSScreen
 * 3. Allocates framebuffers using GX2-compatible memory
 * 4. Performs double clear+flip to initialize both front and back buffers
 * 5. Enables both screens
 * 6. Disables the HOME button menu (to prevent interference)
 *
 * Call this once before starting your rendering loop.
 *
 * @return true if initialization succeeded, false if buffer allocation failed
 *
 * NOTE: If this returns false, you should NOT call any other Screen functions.
 */
bool Init();

/**
 * Shut down the screen rendering system and restore game graphics.
 *
 * This function:
 * 1. Re-enables the HOME button menu
 * 2. Restores display controller registers to their original values
 * 3. Frees allocated framebuffers
 *
 * Always call this when you're done rendering, even if you're about to launch
 * a new title. Failure to call this will leave the game's graphics corrupted.
 */
void Shutdown();

// =============================================================================
// Frame Management
// =============================================================================

/**
 * Begin a new frame.
 *
 * This function:
 * 1. Waits for vertical sync (prevents tearing)
 * 2. Clears both screen buffers to the specified background color
 *
 * Call this at the start of each frame, before drawing anything.
 *
 * @param bgColor Background color in RGBA format (0xRRGGBBAA)
 *                Common values:
 *                  - 0x000000FF = Black
 *                  - 0x1E1E2EFF = Catppuccin Base (dark blue-gray)
 *                  - 0xFFFFFFFF = White
 */
void BeginFrame(uint32_t bgColor);

/**
 * End the current frame and display it.
 *
 * This function:
 * 1. Flushes CPU cache to ensure GPU sees all changes
 * 2. Flips the framebuffers (swaps front and back buffers)
 *
 * Call this after you've drawn everything for the current frame.
 */
void EndFrame();

// =============================================================================
// Drawing Functions
// =============================================================================

/**
 * Draw text at the specified position on both screens.
 *
 * @param col   Column (0 = left edge)
 * @param row   Row (0 = top edge)
 * @param text  Null-terminated string to draw
 *
 * The text will appear at the same character position on both TV and GamePad.
 * Text that extends past the screen edge will be clipped.
 *
 * Example:
 *   Screen::DrawText(0, 0, "Title");           // Top-left corner
 *   Screen::DrawText(10, 5, "Hello World");    // Column 10, Row 5
 */
void DrawText(int col, int row, const char* text);

/**
 * Draw text on TV screen only.
 *
 * @param col   Column (0 = left edge)
 * @param row   Row (0 = top edge)
 * @param text  Null-terminated string to draw
 *
 * Use this for content that only makes sense on the big screen.
 */
void DrawTextTV(int col, int row, const char* text);

/**
 * Draw text on GamePad screen only.
 *
 * @param col   Column (0 = left edge)
 * @param row   Row (0 = top edge)
 * @param text  Null-terminated string to draw
 *
 * Use this for GamePad-specific content (e.g., touch instructions).
 */
void DrawTextDRC(int col, int row, const char* text);

/**
 * Draw formatted text at the specified position on both screens.
 *
 * @param col   Column (0 = left edge)
 * @param row   Row (0 = top edge)
 * @param fmt   printf-style format string
 * @param ...   Format arguments
 *
 * Example:
 *   Screen::DrawTextF(0, 5, "Score: %d", score);
 *   Screen::DrawTextF(0, 6, "Player: %s", playerName);
 */
void DrawTextF(int col, int row, const char* fmt, ...);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Get the TV framebuffer for direct manipulation.
 *
 * This is an advanced function - prefer using DrawText() when possible.
 * The buffer format is 32-bit RGBA, with dimensions based on OSScreen settings.
 *
 * @return Pointer to TV framebuffer, or nullptr if not initialized
 *
 * WARNING: Direct buffer manipulation requires understanding of OSScreen's
 * double-buffering. Changes are made to the back buffer and only visible
 * after EndFrame() is called.
 */
void* GetTVBuffer();

/**
 * Get the GamePad framebuffer for direct manipulation.
 *
 * @return Pointer to DRC framebuffer, or nullptr if not initialized
 */
void* GetDRCBuffer();

/**
 * Get the size of the TV framebuffer in bytes.
 *
 * @return Size in bytes, or 0 if not initialized
 */
uint32_t GetTVBufferSize();

/**
 * Get the size of the GamePad framebuffer in bytes.
 *
 * @return Size in bytes, or 0 if not initialized
 */
uint32_t GetDRCBufferSize();

} // namespace Screen
