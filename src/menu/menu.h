/**
 * Main Menu System
 *
 * This module implements the game launcher menu interface. It handles:
 * - Menu state management (open/close, mode transitions)
 * - Main rendering loop
 * - Input processing
 * - Launching selected titles
 *
 * MENU LAYOUT:
 * ------------
 * The menu uses a split-screen layout:
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ [All] [Favorites] [Category1] ...         ZL/ZR: Categories     │ <- Category bar
 * ├────────────────────────────────┬────────────────────────────────┤
 * │   1. * Game Title A            │  Full Game Name Here           │ <- Title list (left)
 * │   2.   Game Title B            │  ─────────────────────         │ <- Details (right)
 * │ > 3. * Game Title C  ←selected │  Region: USA                   │
 * │   4.   Game Title D            │  Title ID: 00050000...         │
 * │   ...                          │                                │
 * │                                │  X: Edit Title                 │
 * ├────────────────────────────────┴────────────────────────────────┤
 * │ A:Launch B:Close Y:Fav X:Edit +:Settings         [3/47]         │ <- Footer
 * └─────────────────────────────────────────────────────────────────┘
 *
 * MENU MODES:
 * -----------
 * - BROWSE: Normal mode - navigate titles, select to launch
 * - EDIT: Edit mode - modify selected title's categories, etc.
 * - SETTINGS: Settings mode - configure colors, create categories
 *
 * USAGE:
 * ------
 *   // Call these at plugin init/deinit
 *   Menu::Init();
 *   Menu::Shutdown();
 *
 *   // Call when button combo detected
 *   if (buttonComboPressed && Menu::IsSafeToOpen()) {
 *       Menu::Open();  // Blocks until menu closes
 *   }
 */

#pragma once

#include <cstdint>

namespace Menu {

// =============================================================================
// Menu Modes
// =============================================================================

/**
 * Current mode of the menu interface.
 */
enum class Mode {
    BROWSE,     // Normal browsing mode
    EDIT,       // Editing a title (categories, etc.)
    SETTINGS,   // Settings submenu
    DEBUG_GRID  // Debug overlay showing grid dimensions
};

// =============================================================================
// Display Constants
// =============================================================================
// NOTE: Column/width values are now calculated dynamically by Renderer::
// functions to support different screen sizes. These constants are kept
// for reference and for row positions which remain fixed.

// Row positions (fixed across all screen sizes)
constexpr int CATEGORY_ROW = 0;       // Category bar row
constexpr int HEADER_ROW = 1;         // Header/divider row
constexpr int LIST_START_ROW = 2;     // First row of title list
constexpr int LIST_START_COL = 0;     // Title list starts at column 0

// Dynamic layout values - use Renderer:: functions instead:
// - Renderer::GetDividerCol()       - Divider column (30% of screen width)
// - Renderer::GetDetailsPanelCol()  - Details panel start column
// - Renderer::GetListWidth()        - Title list width in columns
// - Renderer::GetVisibleRows()      - Number of visible title rows
// - Renderer::GetFooterRow()        - Footer row position
// - Renderer::GetTitleNameWidth()   - Max title name characters

// =============================================================================
// Lifecycle Functions
// =============================================================================

/**
 * Initialize the menu system.
 *
 * Call once at plugin startup. Sets up internal state.
 */
void Init();

/**
 * Shut down the menu system.
 *
 * Call at plugin shutdown. Closes menu if open and cleans up.
 */
void Shutdown();

// =============================================================================
// Menu State
// =============================================================================

/**
 * Check if the menu is currently open.
 *
 * @return true if the menu is being displayed
 */
bool IsOpen();

/**
 * Check if it's safe to open the menu.
 *
 * This checks various safety conditions:
 * - Not already open
 * - Startup grace period has elapsed
 * - App is in foreground
 * - ProcUI reports app is running normally
 *
 * @return true if Open() can be called safely
 */
bool IsSafeToOpen();

/**
 * Get the current menu mode.
 *
 * @return Current Mode value
 */
Mode GetMode();

// =============================================================================
// Menu Operations
// =============================================================================

/**
 * Open the menu.
 *
 * This function:
 * 1. Takes over screen rendering
 * 2. Runs the menu loop until user exits or selects a game
 * 3. Restores screen rendering to the game
 * 4. Launches selected title if any
 *
 * This call BLOCKS until the menu is closed. The game will be paused
 * while the menu is displayed.
 *
 * Call IsSafeToOpen() before calling this to avoid issues during
 * game loading or transitions.
 */
void Open();

/**
 * Close the menu.
 *
 * Normally called internally when user presses B or selects a game.
 * Can be called externally to force-close (e.g., when app loses foreground).
 */
void Close();

// =============================================================================
// Safety Tracking
// =============================================================================
// These functions are used by main.cpp to track application state for
// the IsSafeToOpen() check.
// =============================================================================

/**
 * Notify menu system that a new application has started.
 *
 * Called from ON_APPLICATION_START hook. Resets the grace period timer.
 */
void OnApplicationStart();

/**
 * Notify menu system that the application has ended.
 *
 * Called from ON_APPLICATION_ENDS hook. Force-closes menu if open.
 */
void OnApplicationEnd();

/**
 * Notify menu system that app acquired foreground.
 *
 * Called from ON_ACQUIRED_FOREGROUND hook.
 */
void OnForegroundAcquired();

/**
 * Notify menu system that app released foreground.
 *
 * Called from ON_RELEASE_FOREGROUND hook. Force-closes menu if open.
 */
void OnForegroundReleased();

} // namespace Menu
