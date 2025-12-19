/**
 * Stub for <sysapp/launch.h>
 * Provides no-op implementations that display status messages.
 */

#pragma once

#include <cstdint>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

inline void SYSLaunchTitle(uint64_t titleId) {
    printf("SYSLaunchTitle: %016llX\n", (unsigned long long)titleId);
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would launch: ' + $0.toString(16).toUpperCase().padStart(16, '0');
            el.className = 'loading';
        }
    }, titleId);
#endif
}

inline void SYSLaunchMenu() {
    printf("SYSLaunchMenu\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would return to Wii U Menu';
            el.className = 'loading';
        }
    });
#endif
}

inline void SYSLaunchMiiStudio(void*) {
    printf("SYSLaunchMiiStudio\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Mii Maker';
            el.className = 'loading';
        }
    });
#endif
}

inline void _SYSLaunchSettings(void*) {
    printf("_SYSLaunchSettings\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open System Settings';
            el.className = 'loading';
        }
    });
#endif
}

inline void _SYSLaunchParental(void*) {
    printf("_SYSLaunchParental\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Parental Controls';
            el.className = 'loading';
        }
    });
#endif
}

inline void _SYSLaunchNotifications(void*) {
    printf("_SYSLaunchNotifications\n");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var el = document.getElementById('status-text');
        if (el) {
            el.textContent = 'Would open Notifications';
            el.className = 'loading';
        }
    });
#endif
}
