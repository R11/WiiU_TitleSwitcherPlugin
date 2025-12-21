/**
 * Miiverse Panel
 * Test panel for Pretendo Miiverse API integration.
 */

#pragma once

#include <cstdint>

namespace Menu {
namespace MiiversePanel {

void Render();
void HandleInput(uint32_t pressed);

// Trigger a test fetch (can be called before entering panel)
void RunTest();

// Reset state for fresh test
void Reset();

}
}
