/**
 * Stub for <coreinit/time.h>
 */

#pragma once

#include <cstdint>

using OSTime = int64_t;

inline OSTime OSGetTime() {
    return 0;
}

inline uint32_t OSTicksToMilliseconds(OSTime ticks) {
    (void)ticks;
    return 10000;  // Return large value to bypass startup grace period
}
