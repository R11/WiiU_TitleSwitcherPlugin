/**
 * Web Input System
 *
 * Maps keyboard events to VPAD button presses.
 * Keyboard mapping:
 *   Arrow keys  -> D-pad
 *   Enter/Space -> A button
 *   Escape/Backspace -> B button
 *   X -> X button
 *   Y -> Y button
 *   Q -> ZL
 *   E -> ZR
 *   Tab -> L
 *   Shift+Tab -> R
 *   + -> Plus
 *   - -> Minus
 */

#include "vpad/input.h"
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

        // Enter/Space -> A
        case 13: return VPAD_BUTTON_A;      // Enter
        case 32: return VPAD_BUTTON_A;      // Space

        // Escape/Backspace -> B
        case 27: return VPAD_BUTTON_B;      // Escape
        case 8:  return VPAD_BUTTON_B;      // Backspace

        // X and Y buttons
        case 88: return VPAD_BUTTON_X;      // X key
        case 89: return VPAD_BUTTON_Y;      // Y key
        case 70: return VPAD_BUTTON_Y;      // F key (Favorite)

        // Shoulder buttons
        case 81: return VPAD_BUTTON_ZL;     // Q
        case 69: return VPAD_BUTTON_ZR;     // E
        case 9:  return VPAD_BUTTON_L;      // Tab (L)
        // Shift+Tab would be R but harder to detect

        // Plus/Minus
        case 187: return VPAD_BUTTON_PLUS;  // + (=)
        case 189: return VPAD_BUTTON_MINUS; // -

        // Numpad
        case 107: return VPAD_BUTTON_PLUS;  // Numpad +
        case 109: return VPAD_BUTTON_MINUS; // Numpad -

        default: return 0;
    }
}

} // anonymous namespace

/**
 * Called from JavaScript when a key is pressed
 */
extern "C" {

void onKeyDown(int keyCode) {
    uint32_t button = keyCodeToButton(keyCode);
    if (button) {
        sHeldButtons |= button;
    }
}

void onKeyUp(int keyCode) {
    uint32_t button = keyCodeToButton(keyCode);
    if (button) {
        sHeldButtons &= ~button;
    }
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
