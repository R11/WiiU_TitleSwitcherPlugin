/**
 * Stub for <notifications/notifications.h>
 */

#pragma once

#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

inline void NotificationModule_AddErrorNotification(const char* msg) {
    printf("ERROR: %s\n", msg);
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Error: ' + UTF8ToString($0);
            el.className = 'error';
        }
    }, msg);
#endif
}
