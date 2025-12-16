/**
 * Buttons Stub for Desktop Preview
 *
 * Provides button definitions without Wii U SDK dependency.
 */

#pragma once

#include "vpad_stub.h"
#include <cstdint>

namespace Buttons {

// Physical button constants
constexpr uint32_t BTN_A     = VPAD_BUTTON_A;
constexpr uint32_t BTN_B     = VPAD_BUTTON_B;
constexpr uint32_t BTN_X     = VPAD_BUTTON_X;
constexpr uint32_t BTN_Y     = VPAD_BUTTON_Y;
constexpr uint32_t BTN_UP    = VPAD_BUTTON_UP;
constexpr uint32_t BTN_DOWN  = VPAD_BUTTON_DOWN;
constexpr uint32_t BTN_LEFT  = VPAD_BUTTON_LEFT;
constexpr uint32_t BTN_RIGHT = VPAD_BUTTON_RIGHT;
constexpr uint32_t BTN_L     = VPAD_BUTTON_L;
constexpr uint32_t BTN_R     = VPAD_BUTTON_R;
constexpr uint32_t BTN_ZL    = VPAD_BUTTON_ZL;
constexpr uint32_t BTN_ZR    = VPAD_BUTTON_ZR;
constexpr uint32_t BTN_PLUS  = VPAD_BUTTON_PLUS;
constexpr uint32_t BTN_MINUS = VPAD_BUTTON_MINUS;
constexpr uint32_t BTN_HOME  = VPAD_BUTTON_HOME;
constexpr uint32_t BTN_STICK_L = VPAD_BUTTON_STICK_L;
constexpr uint32_t BTN_STICK_R = VPAD_BUTTON_STICK_R;

struct Button {
    uint32_t input;
    const char* label;

    constexpr bool Pressed(uint32_t triggered) const {
        return (triggered & input) != 0;
    }

    constexpr bool Held(uint32_t held) const {
        return (held & input) != 0;
    }
};

namespace Actions {

constexpr Button NAV_UP        = { VPAD_BUTTON_UP,    "Up" };
constexpr Button NAV_DOWN      = { VPAD_BUTTON_DOWN,  "Down" };
constexpr Button NAV_SKIP_UP   = { VPAD_BUTTON_LEFT,  "Left" };
constexpr Button NAV_SKIP_DOWN = { VPAD_BUTTON_RIGHT, "Right" };
constexpr Button NAV_PAGE_UP   = { VPAD_BUTTON_L,     "L" };
constexpr Button NAV_PAGE_DOWN = { VPAD_BUTTON_R,     "R" };

constexpr Button CONFIRM       = { VPAD_BUTTON_A,     "A" };
constexpr Button CANCEL        = { VPAD_BUTTON_B,     "B" };
constexpr Button FAVORITE      = { VPAD_BUTTON_Y,     "Y" };
constexpr Button EDIT          = { VPAD_BUTTON_X,     "X" };
constexpr Button SETTINGS      = { VPAD_BUTTON_PLUS,  "+" };

constexpr Button CATEGORY_PREV = { VPAD_BUTTON_ZL,    "ZL" };
constexpr Button CATEGORY_NEXT = { VPAD_BUTTON_ZR,    "ZR" };

constexpr Button INPUT_CONFIRM = { VPAD_BUTTON_PLUS,  "+" };
constexpr Button INPUT_CANCEL  = { VPAD_BUTTON_MINUS, "-" };
constexpr Button INPUT_RIGHT   = { VPAD_BUTTON_A,     "A" };
constexpr Button INPUT_LEFT    = { VPAD_BUTTON_B,     "B" };
constexpr Button INPUT_DELETE  = { VPAD_BUTTON_X,     "X" };
constexpr Button INPUT_CHAR_UP   = { VPAD_BUTTON_UP,   "Up" };
constexpr Button INPUT_CHAR_DOWN = { VPAD_BUTTON_DOWN, "Down" };

constexpr Button PANEL_LEFT    = { VPAD_BUTTON_LEFT,  "Left" };
constexpr Button PANEL_RIGHT   = { VPAD_BUTTON_RIGHT, "Right" };

constexpr uint32_t MENU_OPEN_COMBO = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS;
constexpr const char* MENU_OPEN_COMBO_LABEL = "L+R+-";

} // namespace Actions

namespace Skip {
constexpr int SMALL = 5;
constexpr int LARGE = 15;
} // namespace Skip

inline bool IsComboPressed(uint32_t heldButtons, uint32_t combo) {
    return (heldButtons & combo) == combo;
}

inline bool WasPressed(uint32_t triggeredButtons, uint32_t button) {
    return (triggeredButtons & button) != 0;
}

inline bool IsHeld(uint32_t heldButtons, uint32_t button) {
    return (heldButtons & button) != 0;
}

} // namespace Buttons
