/**
 * Screen Implementation
 */

#include "screen.h"

namespace Screen {

TVResolution GetTVResolution()
{
    // TODO: Detect actual TV resolution from GX2 or system APIs
    // For now, default to 720p as it's the most common
    return TVResolution::P720;
}

} // namespace Screen
