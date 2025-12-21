/**
 * Debug Panel Implementation
 * Debug grid overlay for development.
 */

#include "debug_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../../render/renderer.h"
#include "../../input/buttons.h"

#include <cstdio>

namespace Menu {
namespace DebugPanel {

using namespace Internal;

void Render()
{
    int w = Renderer::GetScreenWidth();
    int h = Renderer::GetScreenHeight();

    // Absolute pixel positions (red)
    Renderer::DrawVLine(0, 0, h, 0xFF0000FF);
    Renderer::DrawVLine(w - 1, 0, h, 0xFF0000FF);
    Renderer::DrawVLine(w / 2, 0, h, 0xFF000080);
    Renderer::DrawHLine(0, 0, w, 0xFF0000FF);
    Renderer::DrawHLine(0, h - 1, w, 0xFF0000FF);
    Renderer::DrawHLine(0, h / 2, w, 0xFF000080);

    // Corner pixels
    Renderer::DrawPixel(0, 0, 0xFF0000FF);
    Renderer::DrawPixel(w - 1, 0, 0xFF0000FF);
    Renderer::DrawPixel(0, h - 1, 0xFF0000FF);
    Renderer::DrawPixel(w - 1, h - 1, 0xFF0000FF);

    // Character grid positions (green)
    Renderer::DrawText(0, 0, "X", 0x00FF00FF);
    Renderer::DrawText(50, 0, "M", 0x00FF00FF);
    Renderer::DrawText(99, 0, "E", 0x00FF00FF);

    // Calculated positions (cyan)
    int calcCol0 = 0 * 8;
    int calcCol50 = 50 * 8;
    int calcCol99 = 99 * 8;

    Renderer::DrawVLine(calcCol0, 50, h - 100, 0x00FFFFFF);
    Renderer::DrawVLine(calcCol50, 50, h - 100, 0x00FFFFFF);
    Renderer::DrawVLine(calcCol99, 50, h - 100, 0x00FFFFFF);

    // Divider position (yellow)
    int dividerCol = Renderer::GetDividerCol();
    int dividerPixel = Renderer::ColToPixelX(dividerCol);
    Renderer::DrawVLine(dividerPixel, 50, h - 100, 0xFFFF00FF);

    // Labels
    Renderer::DrawTextF(1, 3, 0xFFFFFFFF, "SCREEN: %dx%d pixels", w, h);
    Renderer::DrawTextF(1, 4, 0xFFFFFFFF, "GRID: 100x18 chars @ 8x24 = 800x432");
    Renderer::DrawTextF(1, 5, 0xFF8080FF, "RED = actual screen edges (pixel 0, %d, %d)", w / 2, w - 1);
    Renderer::DrawTextF(1, 6, 0x80FFFFFF, "CYAN = calculated col 0,50,99 (pixels 0,400,792)");
    Renderer::DrawTextF(1, 7, 0x80FF80FF, "GREEN X,M,E = where col 0,50,99 actually render");
    Renderer::DrawTextF(1, 8, 0xFFFF80FF, "YELLOW = divider at col %d (pixel %d)", dividerCol, dividerPixel);
    Renderer::DrawText(1, 10, "If X is RIGHT of red edge, OSScreen has LEFT MARGIN", 0xFFFFFFFF);
    Renderer::DrawText(1, 11, "If M is RIGHT of red center, OSScreen centers grid", 0xFFFFFFFF);

    Renderer::DrawTextF(1, 13, 0xCDD6F4FF, "Expected margins: L=%d R=%d (total %d unaccounted)",
                        (w - 800) / 2, (w - 800) / 2, w - 800);
    Renderer::DrawTextF(1, 14, 0xCDD6F4FF, "                  T=%d B=%d (total %d unaccounted)",
                        (h - 432) / 2, (h - 432) / 2, h - 432);

    Renderer::DrawText(1, Renderer::GetGridHeight() - 1, "[B:Back]", 0x888888FF);
}

void HandleInput(uint32_t pressed)
{
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        sCurrentMode = Mode::SETTINGS;
    }
}

}
}
