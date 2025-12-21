/**
 * Web Input System
 *
 * Maps keyboard events to VPAD button presses.
 * Simple letter-key mapping to avoid modifier keys and browser conflicts:
 *
 *   WASD / Arrow keys -> D-pad
 *   J -> A button (confirm)
 *   K -> B button (back)
 *   U -> X button (edit)
 *   I -> Y button (favorite)
 *   Q -> ZL (prev category)
 *   E -> ZR (next category)
 *   O -> L button
 *   P -> R button
 *   M -> Plus (settings menu)
 *   N -> Minus
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
        // WASD + Arrow keys -> D-pad
        case 87: return VPAD_BUTTON_UP;     // W
        case 83: return VPAD_BUTTON_DOWN;   // S
        case 65: return VPAD_BUTTON_LEFT;   // A
        case 68: return VPAD_BUTTON_RIGHT;  // D
        case 37: return VPAD_BUTTON_LEFT;   // ArrowLeft
        case 38: return VPAD_BUTTON_UP;     // ArrowUp
        case 39: return VPAD_BUTTON_RIGHT;  // ArrowRight
        case 40: return VPAD_BUTTON_DOWN;   // ArrowDown

        // Action buttons (JKUI cluster)
        case 74: return VPAD_BUTTON_A;      // J (confirm)
        case 75: return VPAD_BUTTON_B;      // K (back)
        case 85: return VPAD_BUTTON_X;      // U (edit)
        case 73: return VPAD_BUTTON_Y;      // I (favorite)

        // Category buttons
        case 81: return VPAD_BUTTON_ZL;     // Q (prev category)
        case 69: return VPAD_BUTTON_ZR;     // E (next category)

        // Shoulder buttons
        case 79: return VPAD_BUTTON_L;      // O
        case 80: return VPAD_BUTTON_R;      // P

        // Menu buttons
        case 77: return VPAD_BUTTON_PLUS;   // M (settings menu)
        case 78: return VPAD_BUTTON_MINUS;  // N

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
