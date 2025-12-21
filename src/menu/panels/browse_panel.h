/**
 * Browse Panel
 * Main browsing mode - title list and details panel.
 */

#pragma once

#include <cstdint>

namespace Menu {
namespace BrowsePanel {

void Render();
uint64_t HandleInput(uint32_t pressed);

}
}
