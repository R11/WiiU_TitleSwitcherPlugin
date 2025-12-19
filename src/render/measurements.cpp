/**
 * Layout Measurements Implementation
 */

#include "measurements.h"
#include "renderer.h"

namespace Measurements {

int GetIconPixelX(int detailsPanelCol)
{
    int screenWidth = Renderer::GetScreenWidth();
    int gridWidth = Renderer::GetGridWidth();

    // Convert grid column to pixel position, then add margin
    return (screenWidth * detailsPanelCol) / gridWidth + ICON_MARGIN_PX;
}

int GetIconPixelY()
{
    int screenHeight = Renderer::GetScreenHeight();
    int gridHeight = Renderer::GetGridHeight();

    // Convert icon row to pixel position
    return (screenHeight * ICON_ROW) / gridHeight;
}

} // namespace Measurements
