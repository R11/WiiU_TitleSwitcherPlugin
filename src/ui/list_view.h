/**
 * Universal List View Component
 *
 * A reusable list component that provides consistent behavior across all
 * menu screens. Supports selection, scrolling, reordering, deletion, and
 * other common list operations.
 *
 * DESIGN PRINCIPLES:
 * ------------------
 * 1. External state - Caller owns the State struct, allowing persistence
 *    across menu opens/closes and easy state sharing.
 *
 * 2. Configurable features - Enable/disable capabilities via Config flags.
 *    Disabled features don't respond to their button inputs.
 *
 * 3. Callback-based rendering - Each item's display is determined by a
 *    callback, allowing full customization without subclassing.
 *
 * 4. Button abstraction - Uses Buttons::Actions for all input, ensuring
 *    consistent behavior with the rest of the plugin.
 *
 * USAGE:
 * ------
 *   // Setup (once)
 *   ListView::State myListState;
 *   ListView::Config myListConfig = {
 *       .col = 0,
 *       .row = 2,
 *       .width = 30,
 *       .visibleRows = 10,
 *       .canReorder = true,
 *   };
 *
 *   // Each frame - Render
 *   myListState.itemCount = getItemCount();
 *   ListView::Render(myListState, myListConfig, [](int idx, bool sel) {
 *       ListView::ItemView view;
 *       view.text = getItemName(idx);
 *       view.prefix = sel ? "> " : "  ";
 *       return view;
 *   });
 *
 *   // Each frame - Handle input
 *   ListView::HandleInput(myListState, pressed, myListConfig);
 *   auto action = ListView::GetAction(pressed, myListConfig);
 *   switch (action) {
 *       case ListView::Action::CONFIRM: doSomething(); break;
 *       case ListView::Action::CANCEL: goBack(); break;
 *       // ...
 *   }
 */

#pragma once

#include <cstdint>
#include <functional>

namespace UI {
namespace ListView {

// =============================================================================
// Configuration
// =============================================================================

/**
 * Configuration for a list view instance.
 * Controls layout, display options, and which features are enabled.
 */
struct Config {
    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    int col = 0;              // Starting column for rendering
    int row = 0;              // Starting row for rendering
    int width = 30;           // Width in characters (for truncation)
    int visibleRows = 10;     // Number of items visible at once

    // -------------------------------------------------------------------------
    // Display Options
    // -------------------------------------------------------------------------

    bool showLineNumbers = false;       // Show "001." before each item
    bool showScrollIndicators = true;   // Show [UP]/[DOWN] when scrollable

    // -------------------------------------------------------------------------
    // Navigation Behavior
    // -------------------------------------------------------------------------

    bool wrapAround = false;  // Wrap selection at list ends
    int smallSkip = 5;        // Items to skip with Left/Right
    int largeSkip = 15;       // Items to skip with L/R (when not reordering)

    // -------------------------------------------------------------------------
    // Feature Flags
    // -------------------------------------------------------------------------
    // These control which actions are available.
    // When disabled, the corresponding button input is ignored.

    bool canConfirm = true;     // A button triggers CONFIRM action
    bool canCancel = true;      // B button triggers CANCEL action
    bool canReorder = false;    // L/R buttons move selected item up/down
    bool canDelete = false;     // X button triggers DELETE action
    bool canToggle = false;     // A button triggers TOGGLE instead of CONFIRM
    bool canFavorite = false;   // Y button triggers FAVORITE action
};

// =============================================================================
// State
// =============================================================================

/**
 * Mutable state for a list view.
 * Owned by the caller, passed to all ListView functions.
 * Can be persisted to restore list position across menu opens.
 */
struct State {
    int selectedIndex = 0;    // Currently highlighted item (0-based)
    int scrollOffset = 0;     // First visible item index
    int itemCount = 0;        // Total number of items in list

    /**
     * Update item count and clamp selection/scroll to valid range.
     * Call this when the underlying data changes.
     */
    void SetItemCount(int count, int visibleRows);

    /**
     * Ensure selection and scroll offset are within valid bounds.
     * Called automatically by HandleInput, but can be called manually.
     */
    void Clamp(int visibleRows);

    /**
     * Move selection by delta, respecting bounds and scroll.
     */
    void MoveSelection(int delta, int visibleRows, bool wrap);
};

// =============================================================================
// Item Display
// =============================================================================

/**
 * How a single list item should be rendered.
 * Returned by the render callback for each visible item.
 */
struct ItemView {
    const char* text = "";          // Main item text
    const char* prefix = "  ";      // Before text: "> ", "* ", "[X] ", etc.
    const char* suffix = "";        // After text: " (5)", " [hidden]", etc.
    uint32_t textColor = 0xFFFFFFFF;   // Color for main text
    uint32_t prefixColor = 0xFFFFFFFF; // Color for prefix (if different)
    bool dimmed = false;            // Render with reduced visibility
};

/**
 * Callback function type for rendering items.
 *
 * @param index        The item index (0-based, into your data)
 * @param isSelected   True if this item is currently highlighted
 * @return             ItemView describing how to render this item
 */
using RenderCallback = std::function<ItemView(int index, bool isSelected)>;

// =============================================================================
// Actions
// =============================================================================

/**
 * Actions that can be triggered by user input.
 * Returned by GetAction() based on button presses and config flags.
 */
enum class Action {
    NONE,           // No action triggered

    // Standard actions
    CONFIRM,        // A pressed (when canConfirm && !canToggle)
    CANCEL,         // B pressed (when canCancel)
    TOGGLE,         // A pressed (when canToggle)
    DELETE,         // X pressed (when canDelete)
    FAVORITE,       // Y pressed (when canFavorite)

    // Reorder actions (when canReorder)
    MOVE_UP,        // L pressed - move selected item up in list
    MOVE_DOWN,      // R pressed - move selected item down in list
};

// =============================================================================
// Core Functions
// =============================================================================

/**
 * Handle navigation input and update state.
 *
 * Processes D-pad (up/down/left/right) and shoulder buttons (L/R) for
 * navigation. Updates selectedIndex and scrollOffset in state.
 *
 * @param state    List state to update
 * @param pressed  Buttons just pressed this frame (VPADStatus.trigger)
 * @param config   List configuration
 */
void HandleInput(State& state, uint32_t pressed, const Config& config);

/**
 * Get the action triggered by input.
 *
 * Checks action buttons (A/B/X/Y/L/R) against config flags and returns
 * the appropriate action. Call this after HandleInput.
 *
 * @param pressed  Buttons just pressed this frame (VPADStatus.trigger)
 * @param config   List configuration
 * @return         Action triggered, or Action::NONE
 */
Action GetAction(uint32_t pressed, const Config& config);

/**
 * Render the list.
 *
 * Draws visible items by calling the render callback for each.
 * Also draws scroll indicators if enabled and applicable.
 *
 * @param state     Current list state
 * @param config    List configuration
 * @param getItem   Callback to get display info for each item
 */
void Render(const State& state, const Config& config, RenderCallback getItem);

/**
 * Render only the scroll indicators.
 *
 * Shows [UP] at top when scrolled down, [DOWN] at bottom when more below.
 * Called automatically by Render() if showScrollIndicators is true.
 *
 * @param state   Current list state
 * @param config  List configuration
 */
void RenderScrollIndicators(const State& state, const Config& config);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Check if the list is scrollable (has more items than visible rows).
 */
inline bool IsScrollable(const State& state, const Config& config) {
    return state.itemCount > config.visibleRows;
}

/**
 * Check if there are items above the current scroll position.
 */
inline bool CanScrollUp(const State& state) {
    return state.scrollOffset > 0;
}

/**
 * Check if there are items below the current scroll position.
 */
inline bool CanScrollDown(const State& state, const Config& config) {
    return state.scrollOffset + config.visibleRows < state.itemCount;
}

/**
 * Get the currently selected item index.
 * Returns -1 if list is empty.
 */
inline int GetSelectedIndex(const State& state) {
    return state.itemCount > 0 ? state.selectedIndex : -1;
}

} // namespace ListView
} // namespace UI
