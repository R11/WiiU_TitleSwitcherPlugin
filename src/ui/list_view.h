/**
 * Universal List View Component
 */

#pragma once

#include <cstdint>
#include <functional>

namespace UI {
namespace ListView {

struct Config {
    int col = 0;
    int row = 0;
    int width = 30;
    int visibleRows = 10;
    bool showLineNumbers = false;
    bool showScrollIndicators = true;
    bool wrapAround = false;
    int smallSkip = 5;
    int largeSkip = 15;
    bool canConfirm = true;
    bool canCancel = true;
    bool canReorder = false;
    bool canDelete = false;
    bool canToggle = false;
    bool canFavorite = false;
};

struct State {
    int selectedIndex = 0;
    int scrollOffset = 0;
    int itemCount = 0;

    void SetItemCount(int count, int visibleRows);
    void Clamp(int visibleRows);
    void MoveSelection(int delta, int visibleRows, bool wrap);
};

struct ItemView {
    const char* text = "";
    const char* prefix = "  ";
    const char* suffix = "";
    uint32_t textColor = 0xFFFFFFFF;
    uint32_t prefixColor = 0xFFFFFFFF;
    bool dimmed = false;
};

using RenderCallback = std::function<ItemView(int index, bool isSelected)>;

enum class Action {
    NONE,
    CONFIRM,
    CANCEL,
    TOGGLE,
    DELETE,
    FAVORITE,
    MOVE_UP,
    MOVE_DOWN,
};

void HandleInput(State& state, uint32_t pressed, const Config& config);
Action GetAction(uint32_t pressed, const Config& config);
void Render(const State& state, const Config& config, RenderCallback getItem);
void RenderScrollIndicators(const State& state, const Config& config);

inline bool IsScrollable(const State& state, const Config& config) {
    return state.itemCount > config.visibleRows;
}

inline bool CanScrollUp(const State& state) {
    return state.scrollOffset > 0;
}

inline bool CanScrollDown(const State& state, const Config& config) {
    return state.scrollOffset + config.visibleRows < state.itemCount;
}

inline int GetSelectedIndex(const State& state) {
    return state.itemCount > 0 ? state.selectedIndex : -1;
}

Config LeftPanelConfig(int visibleRows = -1);
Config DetailsPanelConfig(int rowOffset, int visibleRows);
Config InputOnlyConfig(int visibleRows);
Config BrowseModeConfig(int visibleRows);
Config EditModeConfig(int visibleRows);

} // namespace ListView
} // namespace UI
