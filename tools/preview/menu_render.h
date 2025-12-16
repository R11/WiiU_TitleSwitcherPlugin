/**
 * Menu Rendering Interface
 *
 * Header for the extracted menu rendering logic.
 * Used by the preview tool to render menu frames.
 */

#pragma once

#include <cstdint>

namespace MenuRender {

// Set the current selection state
void SetSelection(int selectedIndex, int scrollOffset);

// Get current selection state
int GetSelectedIndex();
int GetScrollOffset();

// Render a complete browse mode frame
void renderFrame(uint32_t bgColor);

// Individual drawing functions (for testing)
void renderBrowseMode();
void drawCategoryBar();
void drawDivider();
void drawTitleList();
void drawDetailsPanel();
void drawFooter();

} // namespace MenuRender
