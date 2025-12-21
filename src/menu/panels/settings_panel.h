/**
 * Settings Panel
 * Settings mode with submodes for colors, categories, system apps.
 */

#pragma once

#include <cstdint>

namespace Menu {
namespace SettingsPanel {

void Render();
void HandleInput(uint32_t pressed, uint32_t held);

}
}
