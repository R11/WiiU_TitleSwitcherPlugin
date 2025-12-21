/**
 * VPAD stub for web preview
 * Minimal definitions needed to compile menu code
 */

#pragma once

#include <cstdint>

// VPAD button constants
#define VPAD_BUTTON_A        0x8000
#define VPAD_BUTTON_B        0x4000
#define VPAD_BUTTON_X        0x2000
#define VPAD_BUTTON_Y        0x1000
#define VPAD_BUTTON_LEFT     0x0800
#define VPAD_BUTTON_RIGHT    0x0400
#define VPAD_BUTTON_UP       0x0200
#define VPAD_BUTTON_DOWN     0x0100
#define VPAD_BUTTON_ZL       0x0080
#define VPAD_BUTTON_ZR       0x0040
#define VPAD_BUTTON_L        0x0020
#define VPAD_BUTTON_R        0x0010
#define VPAD_BUTTON_PLUS     0x0008
#define VPAD_BUTTON_MINUS    0x0004
#define VPAD_BUTTON_HOME     0x0002
#define VPAD_BUTTON_SYNC     0x0001
#define VPAD_BUTTON_STICK_L  0x00020000
#define VPAD_BUTTON_STICK_R  0x00010000

// Stick data
struct VPADVec2D {
    float x;
    float y;
};

// Touch data
struct VPADTouchData {
    uint16_t x;
    uint16_t y;
    uint16_t touched;
    uint16_t validity;
};

// Main VPAD status structure
struct VPADStatus {
    uint32_t hold;      // Currently held buttons
    uint32_t trigger;   // Just pressed buttons
    uint32_t release;   // Just released buttons
    VPADVec2D leftStick;
    VPADVec2D rightStick;
    VPADTouchData tpNormal;
    // ... other fields not needed for preview
};

// Channel enum
typedef enum {
    VPAD_CHAN_0 = 0
} VPADChan;

// Read error enum
typedef enum {
    VPAD_READ_SUCCESS = 0,
    VPAD_READ_NO_SAMPLES = -1,
    VPAD_READ_INVALID = -2
} VPADReadError;

// Stub function - actual implementation in web_input.cpp
int32_t VPADRead(VPADChan chan, VPADStatus* buffers, uint32_t count, VPADReadError* error);
