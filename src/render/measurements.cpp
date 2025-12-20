/**
 * Layout Measurements Implementation
 *
 * NOTE: This module is being migrated to the new Layout system.
 * Icon positions now come from Layout::GetCurrentLayout().
 * ROW_OFFSET_* constants remain here for backward compatibility.
 */

#include "measurements.h"
#include "renderer.h"
#include "../ui/layout.h"

namespace Measurements {

int GetIconPixelX(int detailsPanelCol)
{
    // Use new layout system for icon position
    const Layout::PixelLayout& layout = Layout::GetCurrentLayout();
    return layout.details.icon.x;
}

int GetIconPixelY()
{
    // Use new layout system for icon position
    const Layout::PixelLayout& layout = Layout::GetCurrentLayout();
    return layout.details.icon.y;
}

} // namespace Measurements
