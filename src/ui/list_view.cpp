/**
 * Universal List View Component
 */

#include "list_view.h"
#include "input/buttons.h"
#include "render/renderer.h"
#include "render/measurements.h"
#include "menu/menu.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace UI {
namespace ListView {

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

void HandleInput(State& state, uint32_t pressed, const Config& config) {
    if (state.itemCount <= 0) {
        return;
    }

    int delta = 0;

    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        delta = -1;
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        delta = 1;
    }

    if (Buttons::Actions::NAV_SKIP_UP.Pressed(pressed)) {
        delta = -config.smallSkip;
    }
    if (Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed)) {
        delta = config.smallSkip;
    }

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
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (config.canToggle) {
            return Action::TOGGLE;
        }
        if (config.canConfirm) {
            return Action::CONFIRM;
        }
    }

    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        if (config.canCancel) {
            return Action::CANCEL;
        }
    }

    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        if (config.canDelete) {
            return Action::DELETE;
        }
    }

    if (Buttons::Actions::FAVORITE.Pressed(pressed)) {
        if (config.canFavorite) {
            return Action::FAVORITE;
        }
    }

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

void Render(const State& state, const Config& config, RenderCallback getItem) {
    if (!getItem) {
        return;
    }

    if (state.itemCount <= 0) {
        Renderer::DrawText(config.col, config.row, "(empty)");
        return;
    }

    for (int i = 0; i < config.visibleRows; i++) {
        int itemIndex = state.scrollOffset + i;
        if (itemIndex >= state.itemCount) {
            break;
        }

        bool isSelected = (itemIndex == state.selectedIndex);
        ItemView view = getItem(itemIndex, isSelected);

        char line[128];
        int pos = 0;

        if (config.showLineNumbers) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%3d.", itemIndex + 1);
        }

        const char* prefix = view.prefix ? view.prefix : "  ";
        pos += snprintf(line + pos, sizeof(line) - pos, "%s", prefix);

        int prefixLen = strlen(prefix);
        int suffixLen = view.suffix ? strlen(view.suffix) : 0;
        int numberLen = config.showLineNumbers ? 4 : 0;
        int maxTextLen = config.width - prefixLen - suffixLen - numberLen - 1;
        if (maxTextLen < 1) {
            maxTextLen = 1;
        }

        const char* text = view.text ? view.text : "";
        int textLen = strlen(text);
        if (textLen > maxTextLen) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%.*s~", maxTextLen - 1, text);
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, "%-*s", maxTextLen, text);
        }

        if (view.suffix && view.suffix[0] != '\0') {
            snprintf(line + pos, sizeof(line) - pos, "%s", view.suffix);
        }

        uint32_t color = view.dimmed ? 0x888888FF : view.textColor;
        Renderer::DrawText(config.col, config.row + i, line, color);
    }

    if (config.showScrollIndicators) {
        RenderScrollIndicators(state, config);
    }
}

void RenderScrollIndicators(const State& state, const Config& config) {
    if (CanScrollUp(state)) {
        int indicatorCol = config.col + config.width - 4;
        if (indicatorCol < config.col) {
            indicatorCol = config.col;
        }
        Renderer::DrawText(indicatorCol, config.row, "[UP]");
    }

    if (CanScrollDown(state, config)) {
        int indicatorCol = config.col + config.width - 6;
        if (indicatorCol < config.col) {
            indicatorCol = config.col;
        }
        int indicatorRow = config.row + config.visibleRows - 1;
        Renderer::DrawText(indicatorCol, indicatorRow, "[DOWN]");
    }
}

Config LeftPanelConfig(int visibleRows) {
    Config config;
    config.col = Menu::LIST_START_COL;
    config.row = Menu::LIST_START_ROW;
    config.width = Renderer::GetDividerCol() - 1;
    config.visibleRows = (visibleRows > 0) ? visibleRows
                                           : (Renderer::GetFooterRow() - Menu::LIST_START_ROW - 1);
    config.showScrollIndicators = true;
    return config;
}

Config DetailsPanelConfig(int rowOffset, int visibleRows) {
    Config config;
    config.col = Renderer::GetDetailsPanelCol();
    config.row = Menu::LIST_START_ROW + rowOffset;
    config.width = Renderer::GetGridWidth() - Renderer::GetDetailsPanelCol() - 1;
    config.visibleRows = visibleRows;
    config.showScrollIndicators = true;
    return config;
}

Config InputOnlyConfig(int visibleRows) {
    Config config;
    config.visibleRows = visibleRows;
    return config;
}

} // namespace ListView
} // namespace UI
