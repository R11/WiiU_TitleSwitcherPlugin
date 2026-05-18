/**
 * Relative Layout Constants
 *
 * Defines UI row positions using relative offsets.
 * When one element moves, dependent elements auto-adjust.
 *
 * Column positions (divider, details panel) are computed dynamically
 * in Renderer based on screen width - see renderer.cpp GetDividerCol().
 */

#pragma once

#include "screen_constants.h"

namespace Layout {

namespace Rows {
    constexpr int CATEGORY = 1;
    constexpr int HEADER = CATEGORY + 1;
    constexpr int LIST_START = HEADER + 1;

    constexpr int FOOTER = Screen::Grid::ROWS - 1;
    constexpr int CONTENT_ROWS = FOOTER - LIST_START - 1;
}

namespace Cols {
    constexpr int LIST_START = 1;
    constexpr int DIVIDER_PERCENT = 30;
}

}
