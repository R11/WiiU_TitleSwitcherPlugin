/**
 * Grid Panel
 *
 * Icon grid browsing mode - displays games as a grid of icons.
 * Alternative to the list-based browse panel.
 */

#pragma once

#include <cstdint>
#include "../../ui/screen.h"
#include "../../ui/grid_view.h"

namespace Menu {
namespace GridPanel {

/**
 * Initialize grid panel state.
 * Call once at menu open or when switching to grid mode.
 */
void Init();

/**
 * Render the grid view on a specific screen.
 *
 * @param screen Screen descriptor for layout calculations
 * @param contentMode What content to render (GRID or DETAILS)
 */
void Render(const Screen::Descriptor& screen, Screen::ContentMode contentMode);

/**
 * Handle input for grid navigation.
 *
 * @param pressed Buttons that were just pressed
 * @return Title ID to launch (non-zero), or 0 to continue
 */
uint64_t HandleInput(uint32_t pressed);

/**
 * Get current grid state (for external access).
 */
const GridView::State& GetState();

/**
 * Set selection to a specific index.
 */
void SetSelection(int index);

/**
 * Cycle to next icon size preset.
 */
void CycleIconSize();

/**
 * Get current icon size preset.
 */
GridView::IconSize GetIconSize();

/**
 * Set icon size preset.
 */
void SetIconSize(GridView::IconSize size);

}
}
