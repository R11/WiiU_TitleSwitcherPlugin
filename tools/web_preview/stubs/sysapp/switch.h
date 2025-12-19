/**
 * Stub for <sysapp/switch.h>
 */

#pragma once

#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

inline void SYSSwitchToBrowser(void*) {
    printf("SYSSwitchToBrowser\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Internet Browser';
            el.className = 'loading';
        }
    });
#endif
}

inline void SYSSwitchToEShop(void*) {
    printf("SYSSwitchToEShop\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Nintendo eShop';
            el.className = 'loading';
        }
    });
#endif
}

inline void SYSSwitchToSyncControllerOnHBM() {
    printf("SYSSwitchToSyncControllerOnHBM\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Controller Sync';
            el.className = 'loading';
        }
    });
#endif
}
