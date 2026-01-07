/**
 * Web Input System
 *
 * Maps keyboard events to VPAD button presses.
 * Letter keys are reserved for typing in text fields.
 *
 *   Arrow keys -> D-pad navigation
 *   Enter -> A button (confirm)
 *   Backspace -> B button (back)
 *   Delete -> X button (delete)
 *   Tab -> Plus (settings menu)
 *   Escape -> Minus (help)
 *   PageUp/PageDown -> L/R (page navigation)
 *   Home/End -> ZL/ZR (category switch)
 *
 * When text input is active, letter/number keys are passed directly
 * to the text input field for typing.
 */

#include "vpad/input.h"
#include "menu/menu.h"
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

// Current button state
static uint32_t sHeldButtons = 0;
static uint32_t sPrevHeldButtons = 0;

// Key code to VPAD button mapping
uint32_t keyCodeToButton(int keyCode) {
    switch (keyCode) {
        // Arrow keys -> D-pad
        case 37: return VPAD_BUTTON_LEFT;   // ArrowLeft
        case 38: return VPAD_BUTTON_UP;     // ArrowUp
        case 39: return VPAD_BUTTON_RIGHT;  // ArrowRight
        case 40: return VPAD_BUTTON_DOWN;   // ArrowDown

        // Primary actions
        case 13: return VPAD_BUTTON_A;      // Enter (confirm)
        case 8:  return VPAD_BUTTON_B;      // Backspace (back)
        case 46: return VPAD_BUTTON_X;      // Delete (delete/edit)

        // Page navigation
        case 33: return VPAD_BUTTON_L;      // PageUp
        case 34: return VPAD_BUTTON_R;      // PageDown

        // Category switching
        case 36: return VPAD_BUTTON_ZL;     // Home (prev category)
        case 35: return VPAD_BUTTON_ZR;     // End (next category)

        // Menu buttons
        case 9:  return VPAD_BUTTON_PLUS;   // Tab (settings menu)
        case 27: return VPAD_BUTTON_MINUS;  // Escape (help)

        default: return 0;
    }
}

} // anonymous namespace

/**
 * Called from JavaScript when a key is pressed
 */
extern "C" {

void handleKeyDown(int keyCode) {
    uint32_t button = keyCodeToButton(keyCode);
    if (button) {
        sHeldButtons |= button;
    }
}

void handleKeyUp(int keyCode) {
    uint32_t button = keyCodeToButton(keyCode);
    if (button) {
        sHeldButtons &= ~button;
    }
}

/**
 * Handle a typed character (for text input fields).
 * Called from JavaScript with the character code.
 * Returns 1 if character was consumed, 0 otherwise.
 */
int handleTypedChar(int charCode) {
    if (!Menu::IsTextInputActive()) {
        return 0;
    }

    char c = static_cast<char>(charCode);
    return Menu::HandleTypedChar(c) ? 1 : 0;
}

/**
 * Check if text input mode is active.
 * JavaScript can use this to decide whether to send typed characters.
 */
int isTextInputActive() {
    return Menu::IsTextInputActive() ? 1 : 0;
}

} // extern "C"

/**
 * VPADRead implementation for web
 */
int32_t VPADRead(VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error) {
    (void)chan;

    if (!buffers || count == 0) {
        if (error) *error = VPAD_READ_INVALID;
        return 0;
    }

    // Fill in the status
    std::memset(buffers, 0, sizeof(VPADStatus));
    buffers->hold = sHeldButtons;
    buffers->trigger = sHeldButtons & ~sPrevHeldButtons;  // Just pressed
    buffers->release = sPrevHeldButtons & ~sHeldButtons;  // Just released

    // Update previous state
    sPrevHeldButtons = sHeldButtons;

    if (error) *error = VPAD_READ_SUCCESS;
    return 1;
}

/**
 * Get current held buttons (for direct access)
 */
uint32_t WebInput_GetHeld() {
    return sHeldButtons;
}

/**
 * Get just-pressed buttons
 */
uint32_t WebInput_GetTrigger() {
    return sHeldButtons & ~sPrevHeldButtons;
}
