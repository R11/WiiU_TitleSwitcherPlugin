/**
 * Universal List View Component - Implementation
 */

#include "list_view.h"
#include "input/buttons.h"
#include "render/renderer.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace UI {
namespace ListView {

// =============================================================================
// State Implementation
// =============================================================================

void State::SetItemCount(int count, int visibleRows) {
    itemCount = count;
    Clamp(visibleRows);
}

void State::Clamp(int visibleRows) {
    // Clamp selection to valid range
    if (itemCount <= 0) {
        selectedIndex = 0;
        scrollOffset = 0;
        return;
    }

    if (selectedIndex < 0) {
        selectedIndex = 0;
    }
    if (selectedIndex >= itemCount) {
        selectedIndex = itemCount - 1;
    }

    // Ensure selection is visible (adjust scroll)
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    }
    if (selectedIndex >= scrollOffset + visibleRows) {
        scrollOffset = selectedIndex - visibleRows + 1;
    }

    // Clamp scroll offset
    if (scrollOffset < 0) {
        scrollOffset = 0;
    }
    int maxScroll = itemCount - visibleRows;
    if (maxScroll < 0) {
        maxScroll = 0;
    }
    if (scrollOffset > maxScroll) {
        scrollOffset = maxScroll;
    }
}

void State::MoveSelection(int delta, int visibleRows, bool wrap) {
    if (itemCount <= 0) {
        return;
    }

    int newIndex = selectedIndex + delta;

    if (wrap) {
        // Wrap around
        while (newIndex < 0) {
            newIndex += itemCount;
        }
        while (newIndex >= itemCount) {
            newIndex -= itemCount;
        }
    } else {
        // Clamp to bounds
        if (newIndex < 0) {
            newIndex = 0;
        }
        if (newIndex >= itemCount) {
            newIndex = itemCount - 1;
        }
    }

    selectedIndex = newIndex;
    Clamp(visibleRows);
}

// =============================================================================
// Input Handling
// =============================================================================

void HandleInput(State& state, uint32_t pressed, const Config& config) {
    if (state.itemCount <= 0) {
        return;
    }

    int delta = 0;

    // Single step navigation (D-pad up/down)
    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        delta = -1;
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        delta = 1;
    }

    // Small skip navigation (D-pad left/right)
    if (Buttons::Actions::NAV_SKIP_UP.Pressed(pressed)) {
        delta = -config.smallSkip;
    }
    if (Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed)) {
        delta = config.smallSkip;
    }

    // Large skip navigation (L/R) - only when not used for reorder
    if (!config.canReorder) {
        if (Buttons::Actions::NAV_PAGE_UP.Pressed(pressed)) {
            delta = -config.largeSkip;
        }
        if (Buttons::Actions::NAV_PAGE_DOWN.Pressed(pressed)) {
            delta = config.largeSkip;
        }
    }

    if (delta != 0) {
        state.MoveSelection(delta, config.visibleRows, config.wrapAround);
    }
}

Action GetAction(uint32_t pressed, const Config& config) {
    // A button - confirm or toggle
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (config.canToggle) {
            return Action::TOGGLE;
        }
        if (config.canConfirm) {
            return Action::CONFIRM;
        }
    }

    // B button - cancel
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        if (config.canCancel) {
            return Action::CANCEL;
        }
    }

    // X button - delete
    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        if (config.canDelete) {
            return Action::DELETE;
        }
    }

    // Y button - favorite
    if (Buttons::Actions::FAVORITE.Pressed(pressed)) {
        if (config.canFavorite) {
            return Action::FAVORITE;
        }
    }

    // L/R buttons - reorder (when enabled)
    if (config.canReorder) {
        if (Buttons::Actions::NAV_PAGE_UP.Pressed(pressed)) {
            return Action::MOVE_UP;
        }
        if (Buttons::Actions::NAV_PAGE_DOWN.Pressed(pressed)) {
            return Action::MOVE_DOWN;
        }
    }

    return Action::NONE;
}

// =============================================================================
// Rendering
// =============================================================================

void Render(const State& state, const Config& config, RenderCallback getItem) {
    if (!getItem) {
        return;
    }

    // Handle empty list
    if (state.itemCount <= 0) {
        Renderer::DrawText(config.col, config.row, "(empty)");
        return;
    }

    // Render each visible item
    for (int i = 0; i < config.visibleRows; i++) {
        int itemIndex = state.scrollOffset + i;
        if (itemIndex >= state.itemCount) {
            break;
        }

        bool isSelected = (itemIndex == state.selectedIndex);
        ItemView view = getItem(itemIndex, isSelected);

        // Build the display line
        char line[128];
        int pos = 0;

        // Line number (if enabled)
        if (config.showLineNumbers) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%3d.", itemIndex + 1);
        }

        // Prefix (cursor, checkbox, etc.)
        const char* prefix = view.prefix ? view.prefix : "  ";
        pos += snprintf(line + pos, sizeof(line) - pos, "%s", prefix);

        // Calculate max text width
        int prefixLen = strlen(prefix);
        int suffixLen = view.suffix ? strlen(view.suffix) : 0;
        int numberLen = config.showLineNumbers ? 4 : 0;
        int maxTextLen = config.width - prefixLen - suffixLen - numberLen - 1;
        if (maxTextLen < 1) {
            maxTextLen = 1;
        }

        // Main text (truncated to fit)
        const char* text = view.text ? view.text : "";
        int textLen = strlen(text);
        if (textLen > maxTextLen) {
            // Truncate with indicator
            pos += snprintf(line + pos, sizeof(line) - pos, "%.*s~", maxTextLen - 1, text);
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, "%-*s", maxTextLen, text);
        }

        // Suffix
        if (view.suffix && view.suffix[0] != '\0') {
            snprintf(line + pos, sizeof(line) - pos, "%s", view.suffix);
        }

        // Draw the line
        uint32_t color = view.dimmed ? 0x888888FF : view.textColor;
        Renderer::DrawText(config.col, config.row + i, line, color);
    }

    // Scroll indicators
    if (config.showScrollIndicators) {
        RenderScrollIndicators(state, config);
    }
}

void RenderScrollIndicators(const State& state, const Config& config) {
    // Show [UP] indicator when there are items above
    if (CanScrollUp(state)) {
        int indicatorCol = config.col + config.width - 4;
        if (indicatorCol < config.col) {
            indicatorCol = config.col;
        }
        Renderer::DrawText(indicatorCol, config.row, "[UP]");
    }

    // Show [DOWN] indicator when there are items below
    if (CanScrollDown(state, config)) {
        int indicatorCol = config.col + config.width - 6;
        if (indicatorCol < config.col) {
            indicatorCol = config.col;
        }
        int indicatorRow = config.row + config.visibleRows - 1;
        Renderer::DrawText(indicatorCol, indicatorRow, "[DOWN]");
    }
}

} // namespace ListView
} // namespace UI
