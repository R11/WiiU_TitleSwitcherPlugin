/**
 * Help Overlay
 * Context-sensitive help overlay triggered by minus button.
 */

#pragma once

#include <cstdint>

namespace Menu {
namespace HelpOverlay {

enum class Context {
    BROWSE,
    EDIT,
    SETTINGS_MAIN,
    SETTINGS_MANAGE_CATS,
    SETTINGS_SYSTEM_APPS,
    SETTINGS_COLORS,
    SETTINGS_COLOR_INPUT,
    SETTINGS_NAME_INPUT,
    DEBUG_GRID
};

bool IsVisible();
void Show(Context context);
void Hide();
void Toggle(Context context);

void Render();
bool HandleInput(uint32_t pressed);

void DrawHelpIndicator();

}
}
