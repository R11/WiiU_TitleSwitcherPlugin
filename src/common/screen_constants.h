/**
 * Shared Screen Constants
 *
 * Single source of truth for screen dimensions and grid parameters.
 * Used by both main plugin code and web_preview.
 */

#pragma once

#include <cstdint>

namespace Screen {

namespace DRC {
    constexpr int WIDTH = 854;
    constexpr int HEIGHT = 480;
}

namespace TV {
    namespace P1080 {
        constexpr int WIDTH = 1920;
        constexpr int HEIGHT = 1080;
    }
    namespace P720 {
        constexpr int WIDTH = 1280;
        constexpr int HEIGHT = 720;
    }
    namespace P480 {
        constexpr int WIDTH = 640;
        constexpr int HEIGHT = 480;
    }
}

namespace Grid {
    constexpr int COLS = 100;
    constexpr int ROWS = 20;
    constexpr int CHAR_WIDTH = 8;
    constexpr int CHAR_HEIGHT = 24;

    constexpr int TOTAL_PIXEL_WIDTH = COLS * CHAR_WIDTH;
    constexpr int TOTAL_PIXEL_HEIGHT = ROWS * CHAR_HEIGHT;
}

}
