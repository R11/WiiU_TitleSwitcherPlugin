/**
 * Grid View System
 *
 * Provides an icon-based grid layout for game selection as an alternative
 * to the traditional list view. Supports multiple icon size presets and
 * adapts to any screen resolution via the Screen::Descriptor system.
 *
 * USAGE:
 * ------
 *   // Compute layout for current screen
 *   Screen::Descriptor screen = Screen::GetDescriptor(Screen::Target::DRC);
 *   GridView::Layout layout = GridView::ComputeLayout(screen, GridView::IconSize::MEDIUM);
 *
 *   // Initialize state
 *   GridView::State state;
 *   state.selectedIndex = 0;
 *   state.scrollRow = 0;
 *   state.totalItems = Titles::GetCount();
 *
 *   // Handle navigation
 *   GridView::HandleNavigation(state, layout, buttonsTriggered);
 */

#pragma once

#include <cstdint>
#include "screen.h"

namespace GridView {

// Icon size presets (similar to 3DS home menu)
enum class IconSize {
    SMALL,      // More titles visible, compact grid
    MEDIUM,     // Balanced (default)
    LARGE,      // Fewer titles, bigger icons

    COUNT
};

// Get display label for icon size
inline const char* GetIconSizeLabel(IconSize size) {
    switch (size) {
        case IconSize::SMALL:  return "Small";
        case IconSize::MEDIUM: return "Medium";
        case IconSize::LARGE:  return "Large";
        default: return "Unknown";
    }
}

/**
 * Grid Layout
 *
 * Computed layout parameters for rendering the grid on a specific screen.
 * All values are in pixels, pre-scaled for the target screen.
 */
struct Layout {
    // Grid dimensions
    int columns;                // Number of columns
    int visibleRows;            // Number of visible rows

    // Cell dimensions
    int cellWidth;              // Width of each cell (icon + padding)
    int cellHeight;             // Height of each cell (icon + padding)
    int iconSize;               // Icon dimension (square)
    int cellPadding;            // Padding around icon within cell

    // Grid positioning
    int gridStartX;             // Left edge of grid content
    int gridStartY;             // Top edge of grid content
    int gridWidth;              // Total grid width
    int gridHeight;             // Total grid height

    // UI element positions
    int categoryBarY;           // Y position for category bar
    int categoryBarHeight;      // Height of category bar
    int titleBarY;              // Y position for selected title name
    int titleBarHeight;         // Height of title bar

    // Selection styling
    int selectionBorderWidth;   // Width of selection highlight border

    // Helpers
    int getTotalVisibleCells() const { return columns * visibleRows; }

    int getCellX(int column) const {
        return gridStartX + (column * cellWidth);
    }

    int getCellY(int row) const {
        return gridStartY + (row * cellHeight);
    }

    int getIconX(int column) const {
        return getCellX(column) + (cellWidth - iconSize) / 2;
    }

    int getIconY(int row) const {
        return getCellY(row) + (cellHeight - iconSize) / 2;
    }
};

/**
 * Grid State
 *
 * Runtime state for grid navigation and scrolling.
 */
struct State {
    int selectedIndex;      // Currently selected title (linear index into title list)
    int scrollRow;          // First visible row (for vertical scrolling)
    int totalItems;         // Total number of items in the grid

    State() : selectedIndex(0), scrollRow(0), totalItems(0) {}

    // Get column of selected item
    int selectedCol(int columns) const {
        return (columns > 0) ? (selectedIndex % columns) : 0;
    }

    // Get row of selected item (absolute, not relative to scroll)
    int selectedRow(int columns) const {
        return (columns > 0) ? (selectedIndex / columns) : 0;
    }

    // Get total number of rows
    int totalRows(int columns) const {
        if (columns <= 0 || totalItems <= 0) return 0;
        return (totalItems + columns - 1) / columns;
    }

    // Check if selection is visible
    bool isSelectionVisible(const Layout& layout) const {
        int row = selectedRow(layout.columns);
        return row >= scrollRow && row < scrollRow + layout.visibleRows;
    }
};

/**
 * Base icon sizes for each preset (at scale 1.0 / DRC resolution).
 * These are scaled by Screen::Descriptor::scale for other resolutions.
 */
struct IconSizeConfig {
    int iconSize;       // Base icon dimension
    int cellPadding;    // Base padding around icon
};

constexpr IconSizeConfig ICON_SIZE_CONFIGS[] = {
    { 96,  12 },    // SMALL
    { 128, 16 },    // MEDIUM
    { 160, 20 },    // LARGE
};

/**
 * Compute grid layout for a screen with given icon size preset.
 *
 * @param screen Screen descriptor (dimensions, scale)
 * @param iconSize Icon size preset
 * @return Computed layout for rendering
 */
Layout ComputeLayout(const Screen::Descriptor& screen, IconSize iconSize);

/**
 * Handle D-pad navigation in the grid.
 *
 * @param state Grid state to update
 * @param layout Current layout
 * @param buttonsTriggered VPAD buttons that were just pressed
 * @return true if state changed
 */
bool HandleNavigation(State& state, const Layout& layout, uint32_t buttonsTriggered);

/**
 * Ensure selection is visible by adjusting scroll position.
 *
 * @param state Grid state to update
 * @param layout Current layout
 */
void EnsureSelectionVisible(State& state, const Layout& layout);

/**
 * Clamp state values to valid ranges.
 *
 * @param state Grid state to clamp
 * @param layout Current layout
 */
void ClampState(State& state, const Layout& layout);

/**
 * Set selection to a specific index, adjusting scroll as needed.
 *
 * @param state Grid state to update
 * @param layout Current layout
 * @param index New selected index
 */
void SetSelection(State& state, const Layout& layout, int index);

} // namespace GridView
