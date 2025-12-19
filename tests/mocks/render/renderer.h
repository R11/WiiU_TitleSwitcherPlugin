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

// Layout helpers (mock values for testing)
inline int GetGridWidth() { return 100; }
inline int GetGridHeight() { return 18; }
inline int GetDividerCol() { return 30; }
inline int GetDetailsPanelCol() { return 32; }
inline int GetListWidth() { return 30; }
inline int GetVisibleRows() { return 15; }
inline int GetFooterRow() { return 17; }
inline int GetScreenWidth() { return 854; }
inline int GetScreenHeight() { return 480; }

} // namespace Renderer
