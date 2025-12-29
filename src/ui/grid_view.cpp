/**
 * Grid View Implementation
 */

#include "grid_view.h"
#include "../input/buttons.h"
#include <algorithm>

namespace GridView {

Layout ComputeLayout(const Screen::Descriptor& screen, IconSize iconSize) {
    Layout layout;

    // Get base sizes for this preset and scale for screen
    int presetIndex = static_cast<int>(iconSize);
    const IconSizeConfig& config = ICON_SIZE_CONFIGS[presetIndex];

    layout.iconSize = screen.scaled(config.iconSize);
    layout.cellPadding = screen.scaled(config.cellPadding);
    layout.cellWidth = layout.iconSize + (layout.cellPadding * 2);
    layout.cellHeight = layout.iconSize + (layout.cellPadding * 2);
    layout.selectionBorderWidth = screen.scaled(3);

    // Reserve space for category bar at top and title bar at bottom
    layout.categoryBarHeight = screen.scaled(32);
    layout.titleBarHeight = screen.scaled(28);

    layout.categoryBarY = screen.contentY();
    layout.titleBarY = screen.height - screen.marginY - layout.titleBarHeight;

    // Grid content area
    int gridTopMargin = screen.scaled(8);
    int gridBottomMargin = screen.scaled(8);

    layout.gridStartY = layout.categoryBarY + layout.categoryBarHeight + gridTopMargin;
    int gridEndY = layout.titleBarY - gridBottomMargin;
    int availableHeight = gridEndY - layout.gridStartY;

    // Calculate grid dimensions
    int availableWidth = screen.contentWidth();

    layout.columns = availableWidth / layout.cellWidth;
    layout.visibleRows = availableHeight / layout.cellHeight;

    // Ensure at least 1 column and 1 row
    if (layout.columns < 1) layout.columns = 1;
    if (layout.visibleRows < 1) layout.visibleRows = 1;

    // Calculate actual grid size and center horizontally
    layout.gridWidth = layout.columns * layout.cellWidth;
    layout.gridHeight = layout.visibleRows * layout.cellHeight;
    layout.gridStartX = screen.contentX() + (availableWidth - layout.gridWidth) / 2;

    return layout;
}

bool HandleNavigation(State& state, const Layout& layout, uint32_t buttonsTriggered) {
    if (state.totalItems == 0) return false;

    int oldIndex = state.selectedIndex;
    int columns = layout.columns;
    int totalRows = state.totalRows(columns);

    // D-pad navigation (Left/Right move between columns)
    if (Buttons::Actions::NAV_SKIP_UP.Pressed(buttonsTriggered)) {
        if (state.selectedIndex % columns > 0) {
            state.selectedIndex--;
        }
    }

    if (Buttons::Actions::NAV_SKIP_DOWN.Pressed(buttonsTriggered)) {
        if (state.selectedIndex % columns < columns - 1 &&
            state.selectedIndex + 1 < state.totalItems) {
            state.selectedIndex++;
        }
    }

    // Up/Down move between rows
    if (Buttons::Actions::NAV_UP.Pressed(buttonsTriggered)) {
        if (state.selectedIndex >= columns) {
            state.selectedIndex -= columns;
        }
    }

    if (Buttons::Actions::NAV_DOWN.Pressed(buttonsTriggered)) {
        int nextIndex = state.selectedIndex + columns;
        if (nextIndex < state.totalItems) {
            state.selectedIndex = nextIndex;
        } else if (state.selectedRow(columns) < totalRows - 1) {
            // Move to last item if on incomplete last row
            state.selectedIndex = state.totalItems - 1;
        }
    }

    // Page navigation (L/R shoulder buttons)
    if (Buttons::Actions::NAV_PAGE_UP.Pressed(buttonsTriggered)) {
        int pageJump = columns * layout.visibleRows;
        state.selectedIndex = std::max(0, state.selectedIndex - pageJump);
    }

    if (Buttons::Actions::NAV_PAGE_DOWN.Pressed(buttonsTriggered)) {
        int pageJump = columns * layout.visibleRows;
        state.selectedIndex = std::min(state.totalItems - 1, state.selectedIndex + pageJump);
    }

    // Update scroll position if selection changed
    if (state.selectedIndex != oldIndex) {
        EnsureSelectionVisible(state, layout);
        return true;
    }

    return false;
}

void EnsureSelectionVisible(State& state, const Layout& layout) {
    if (layout.columns <= 0) return;

    int selectedRow = state.selectedRow(layout.columns);

    // Scroll up if selection is above viewport
    if (selectedRow < state.scrollRow) {
        state.scrollRow = selectedRow;
    }

    // Scroll down if selection is below viewport
    if (selectedRow >= state.scrollRow + layout.visibleRows) {
        state.scrollRow = selectedRow - layout.visibleRows + 1;
    }

    // Clamp scroll row
    int maxScrollRow = state.totalRows(layout.columns) - layout.visibleRows;
    if (maxScrollRow < 0) maxScrollRow = 0;
    if (state.scrollRow > maxScrollRow) state.scrollRow = maxScrollRow;
    if (state.scrollRow < 0) state.scrollRow = 0;
}

void ClampState(State& state, const Layout& layout) {
    // Clamp selected index
    if (state.totalItems <= 0) {
        state.selectedIndex = 0;
        state.scrollRow = 0;
        return;
    }

    if (state.selectedIndex >= state.totalItems) {
        state.selectedIndex = state.totalItems - 1;
    }
    if (state.selectedIndex < 0) {
        state.selectedIndex = 0;
    }

    // Ensure selection is visible
    EnsureSelectionVisible(state, layout);
}

void SetSelection(State& state, const Layout& layout, int index) {
    state.selectedIndex = index;
    ClampState(state, layout);
}

} // namespace GridView
