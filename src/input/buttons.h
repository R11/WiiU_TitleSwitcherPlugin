/**
 * Button Mapping System
 *
 * This file centralizes all button definitions used throughout the plugin.
 * It provides a clean abstraction between physical buttons and their actions,
 * making it easy to:
 *
 * 1. Change which button does what without modifying action code
 * 2. Display consistent labels to users (e.g., in help text)
 * 3. Support button combo detection
 *
 * HOW TO USE:
 * -----------
 * Instead of checking raw VPAD_BUTTON_* constants throughout the code,
 * use the semantic constants defined here:
 *
 *   // Instead of: if (pressed & VPAD_BUTTON_A)
 *   if (pressed & Buttons::ACTION_CONFIRM)
 *
 * To display button labels to users:
 *
 *   char hint[64];
 *   snprintf(hint, sizeof(hint), "%s: Select", Buttons::Labels::CONFIRM);
 *   // Result: "A: Select"
 *
 * TO CHANGE BUTTON MAPPINGS:
 * --------------------------
 * Simply modify the constants in the "Action Mappings" section below.
 * The labels in the "Labels" namespace should match the button names
 * shown on the physical Wii U GamePad.
 *
 * REFERENCE:
 * ----------
 * VPAD button constants are defined in: <vpad/input.h>
 * Documentation: https://wut.devkitpro.org/group__vpad__input.html
 *
 * Physical GamePad button layout:
 *
 *        [ZL]                                      [ZR]
 *        [L]                                       [R]
 *
 *              [UP]                          [X]
 *        [LEFT]    [RIGHT]            [Y]        [A]
 *             [DOWN]                        [B]
 *
 *             [-]    [HOME]   [+]
 *
 *              [Left Stick]   [Right Stick]
 *
 * Note: Stick clicks are STICK_L and STICK_R
 */

#pragma once

#include <vpad/input.h>

namespace Buttons {

// =============================================================================
// Physical Button Constants
// =============================================================================
// These are just aliases for clarity - they map directly to VPAD constants.
// Use these when you need to reference the physical button specifically.
// =============================================================================

// Face buttons (right side of GamePad)
constexpr uint32_t BTN_A     = VPAD_BUTTON_A;      // Green button (right)
constexpr uint32_t BTN_B     = VPAD_BUTTON_B;      // Red button (bottom)
constexpr uint32_t BTN_X     = VPAD_BUTTON_X;      // Blue button (top)
constexpr uint32_t BTN_Y     = VPAD_BUTTON_Y;      // Orange/Yellow button (left)

// D-Pad (left side of GamePad)
constexpr uint32_t BTN_UP    = VPAD_BUTTON_UP;
constexpr uint32_t BTN_DOWN  = VPAD_BUTTON_DOWN;
constexpr uint32_t BTN_LEFT  = VPAD_BUTTON_LEFT;
constexpr uint32_t BTN_RIGHT = VPAD_BUTTON_RIGHT;

// Shoulder buttons (top of GamePad)
constexpr uint32_t BTN_L     = VPAD_BUTTON_L;      // Left bumper
constexpr uint32_t BTN_R     = VPAD_BUTTON_R;      // Right bumper
constexpr uint32_t BTN_ZL    = VPAD_BUTTON_ZL;     // Left trigger
constexpr uint32_t BTN_ZR    = VPAD_BUTTON_ZR;     // Right trigger

// Center buttons
constexpr uint32_t BTN_PLUS  = VPAD_BUTTON_PLUS;   // + button (Start)
constexpr uint32_t BTN_MINUS = VPAD_BUTTON_MINUS;  // - button (Select)
constexpr uint32_t BTN_HOME  = VPAD_BUTTON_HOME;   // HOME button

// Stick buttons (clicking the analog sticks)
constexpr uint32_t BTN_STICK_L = VPAD_BUTTON_STICK_L;  // Left stick click
constexpr uint32_t BTN_STICK_R = VPAD_BUTTON_STICK_R;  // Right stick click

// =============================================================================
// Button Type
// =============================================================================
// A Button combines an input constant with its display label.
// This ensures labels automatically match when you remap buttons.
// =============================================================================

/**
 * Represents a button mapping with both its input code and display label.
 */
struct Button {
    uint32_t input;        // VPAD button constant
    const char* label;     // Display label (e.g., "A", "+", "ZL")

    // Check if this button was pressed
    constexpr bool Pressed(uint32_t triggered) const {
        return (triggered & input) != 0;
    }

    // Check if this button is held
    constexpr bool Held(uint32_t held) const {
        return (held & input) != 0;
    }
};

// =============================================================================
// Action Mappings
// =============================================================================
// These map semantic actions to physical buttons.
// CHANGE THESE to remap buttons throughout the entire plugin!
// Both the input AND label update together automatically.
// =============================================================================

namespace Actions {

// --- General Navigation ---

// Move selection up in a list
constexpr Button NAV_UP        = { VPAD_BUTTON_UP,    "Up" };

// Move selection down in a list
constexpr Button NAV_DOWN      = { VPAD_BUTTON_DOWN,  "Down" };

// Skip multiple items up (small skip)
constexpr Button NAV_SKIP_UP   = { VPAD_BUTTON_LEFT,  "Left" };

// Skip multiple items down (small skip)
constexpr Button NAV_SKIP_DOWN = { VPAD_BUTTON_RIGHT, "Right" };

// Skip many items up (large skip)
constexpr Button NAV_PAGE_UP   = { VPAD_BUTTON_L,     "L" };

// Skip many items down (large skip)
constexpr Button NAV_PAGE_DOWN = { VPAD_BUTTON_R,     "R" };

// --- Menu Actions ---

// Confirm selection / Launch game
constexpr Button CONFIRM       = { VPAD_BUTTON_A,     "A" };

// Cancel / Close menu / Go back
constexpr Button CANCEL        = { VPAD_BUTTON_B,     "B" };

// Toggle favorite status
constexpr Button FAVORITE      = { VPAD_BUTTON_Y,     "Y" };

// Edit selected item / Open edit mode
constexpr Button EDIT          = { VPAD_BUTTON_X,     "X" };

// Open settings menu
constexpr Button SETTINGS      = { VPAD_BUTTON_PLUS,  "+" };

// --- Category Navigation ---

// Previous category
constexpr Button CATEGORY_PREV = { VPAD_BUTTON_ZL,    "ZL" };

// Next category
constexpr Button CATEGORY_NEXT = { VPAD_BUTTON_ZR,    "ZR" };

// --- Text Input Actions ---
// Used when a text input field is focused

// Confirm input and save value
constexpr Button INPUT_CONFIRM = { VPAD_BUTTON_PLUS,  "+" };

// Cancel input and discard changes
constexpr Button INPUT_CANCEL  = { VPAD_BUTTON_MINUS, "-" };

// Move cursor right in input field
constexpr Button INPUT_RIGHT   = { VPAD_BUTTON_A,     "A" };

// Move cursor left in input field
constexpr Button INPUT_LEFT    = { VPAD_BUTTON_B,     "B" };

// Delete character at cursor (backspace)
constexpr Button INPUT_DELETE  = { VPAD_BUTTON_X,     "X" };

// Cycle to next character (alphabetically)
constexpr Button INPUT_CHAR_UP   = { VPAD_BUTTON_UP,   "Up" };

// Cycle to previous character (alphabetically)
constexpr Button INPUT_CHAR_DOWN = { VPAD_BUTTON_DOWN, "Down" };

// --- Panel Navigation ---
// Used when menu has left/right panels (edit mode, settings)

// Move focus to left panel
constexpr Button PANEL_LEFT    = { VPAD_BUTTON_LEFT,  "Left" };

// Move focus to right panel
constexpr Button PANEL_RIGHT   = { VPAD_BUTTON_RIGHT, "Right" };

// --- Special Combos ---

// Button combination to open the menu (all must be held)
// Note: This is a bitmask of buttons that must ALL be pressed
constexpr uint32_t MENU_OPEN_COMBO = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS;
constexpr const char* MENU_OPEN_COMBO_LABEL = "L+R+-";

} // namespace Actions

// =============================================================================
// Skip Amounts
// =============================================================================
// How many items to skip when using NAV_SKIP_* and NAV_PAGE_* actions.
// These can be adjusted based on how many titles are typically visible.
// =============================================================================

namespace Skip {

// Items to skip with NAV_SKIP_UP/DOWN (Left/Right by default)
constexpr int SMALL = 5;

// Items to skip with NAV_PAGE_UP/DOWN (L/R by default)
constexpr int LARGE = 15;

} // namespace Skip

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Check if a button combo is fully pressed.
 * All buttons in the combo must be held simultaneously.
 *
 * @param heldButtons  The buttons currently held (from VPADStatus.hold)
 * @param combo        The button combination to check for
 * @return             true if all buttons in combo are held
 *
 * Example:
 *   if (IsComboPressed(vpadStatus.hold, Actions::MENU_OPEN_COMBO)) {
 *       // L + R + Minus are all held
 *   }
 */
inline bool IsComboPressed(uint32_t heldButtons, uint32_t combo)
{
    return (heldButtons & combo) == combo;
}

/**
 * Check if a specific button was just pressed this frame.
 *
 * @param triggeredButtons  The buttons just pressed (from VPADStatus.trigger)
 * @param button            The button to check for
 * @return                  true if the button was just pressed
 *
 * Example:
 *   if (WasPressed(vpadStatus.trigger, Actions::CONFIRM)) {
 *       // A button was just pressed
 *   }
 */
inline bool WasPressed(uint32_t triggeredButtons, uint32_t button)
{
    return (triggeredButtons & button) != 0;
}

/**
 * Check if a specific button is currently held.
 *
 * @param heldButtons  The buttons currently held (from VPADStatus.hold)
 * @param button       The button to check for
 * @return             true if the button is held
 *
 * Example:
 *   if (IsHeld(vpadStatus.hold, BTN_ZL)) {
 *       // ZL is being held down
 *   }
 */
inline bool IsHeld(uint32_t heldButtons, uint32_t button)
{
    return (heldButtons & button) != 0;
}

} // namespace Buttons
