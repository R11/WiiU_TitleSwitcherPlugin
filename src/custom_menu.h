/**
 * Custom Menu - Direct OSScreen rendering for Title Switcher
 *
 * Triggered by L + R + Minus button combo.
 * Uses proper GX2 synchronization to avoid garbled rendering.
 */

#pragma once

#include <cstdint>

namespace CustomMenu {

// Initialize the custom menu system (call once at plugin init)
void Init();

// Preload title list in background (call at plugin init for fast menu open)
void PreloadTitles();

// Cleanup (call at plugin deinit)
void Shutdown();

// Check if menu is currently open
bool IsOpen();

// Open the menu - called when button combo detected
void Open();

// Close the menu
void Close();

// Process input and render - call this from VPADRead hook when menu is open
// Returns true if input was consumed (should block game input)
bool Update(uint32_t buttonsTriggered, uint32_t buttonsHeld);

} // namespace CustomMenu
