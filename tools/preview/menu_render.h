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

// Set settings mode state
void SetSettingsIndex(int index);
void SetSystemAppIndex(int index);
int GetSettingsIndex();
int GetSystemAppIndex();

// Render a complete browse mode frame
void renderFrame(uint32_t bgColor);

// Individual drawing functions (for testing)
void renderBrowseMode();
void drawCategoryBar();
void drawDivider();
void drawTitleList();
void drawDetailsPanel();
void drawFooter();

// Settings mode rendering
void renderSettingsMain();
void renderSystemApps();

} // namespace MenuRender
