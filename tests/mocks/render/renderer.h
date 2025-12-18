/**
 * Mock Renderer for unit tests
 */

#pragma once

#include <cstdint>

namespace Renderer {

inline void DrawText(int col, int row, const char* text, uint32_t color = 0xFFFFFFFF) {
    (void)col; (void)row; (void)text; (void)color;
}

inline void DrawTextF(int col, int row, uint32_t color, const char* fmt, ...) {
    (void)col; (void)row; (void)color; (void)fmt;
}

inline void DrawTextF(int col, int row, const char* fmt, ...) {
    (void)col; (void)row; (void)fmt;
}

} // namespace Renderer
