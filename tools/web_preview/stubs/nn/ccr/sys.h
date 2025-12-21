/**
 * Stub for <nn/ccr/sys.h>
 * GamePad LCD brightness control
 */

#pragma once

enum CCRSysLCDMode {
    CCR_SYS_LCD_MODE_1 = 0,
    CCR_SYS_LCD_MODE_2 = 1,
    CCR_SYS_LCD_MODE_3 = 2,
    CCR_SYS_LCD_MODE_4 = 3,
    CCR_SYS_LCD_MODE_5 = 4
};

inline int CCRSysGetCurrentLCDMode(CCRSysLCDMode* mode) {
    if (mode) {
        *mode = CCR_SYS_LCD_MODE_3;  // Default to middle brightness
    }
    return 0;  // Success
}

inline void CCRSysSetCurrentLCDMode(CCRSysLCDMode mode) {
    (void)mode;
    // No-op in web preview
}
