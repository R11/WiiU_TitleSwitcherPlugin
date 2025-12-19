/**
 * Web Preview Main Entry Point
 *
 * Provides an interactive menu preview in the browser using WebAssembly.
 * Uses the actual menu.cpp state machine from the plugin.
 * Renders to both TV and GamePad (DRC) screens simultaneously.
 */

#include "stubs/renderer_stub.h"
#include "stubs/settings_stub.h"
#include "stubs/titles_stub.h"
#include "menu/menu.h"
#include "menu/categories.h"
#include "vpad/input.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdio>

// Forward declarations from web_input.cpp
extern "C" {
    void onKeyDown(int keyCode);
    void onKeyUp(int keyCode);
}

namespace {

/**
 * Main loop function called by Emscripten
 */
void mainLoop() {
    // Render to DRC (GamePad)
    Renderer::SelectScreen(0);
    Menu::RenderFrame();

    // Render to TV
    Renderer::SelectScreen(1);
    Menu::RenderFrame();

    // Push both framebuffers to canvases
    Renderer::EndFrame();

    // Handle input
    Menu::FrameResult result = Menu::HandleInputFrame();

    // If menu wants to close (launch title or cancel), reset to browse
    if (!result.shouldContinue) {
        if (result.titleToLaunch != 0) {
            printf("Would launch: %016llX\n", (unsigned long long)result.titleToLaunch);
        }
        // In web preview, just reset to browse mode instead of closing
        Menu::ResetToBrowse();
    }
}

} // anonymous namespace

// =============================================================================
// Entry Point
// =============================================================================

int main() {
    printf("Wii U Title Switcher - Web Preview (Dual Screen)\n");
    printf("=================================================\n\n");

    // Initialize renderer
    if (!Renderer::Init()) {
        printf("ERROR: Failed to initialize renderer\n");
        return 1;
    }

    // Initialize settings and titles (uses mocks)
    Settings::Init();
    Settings::Load();
    Titles::Load();

    // Initialize categories
    Categories::Init();

    // Initialize menu for web preview (sets up state without blocking)
    Menu::InitForWebPreview();

    printf("Loaded %d titles\n", Titles::GetCount());
    printf("Controls: WASD=move, J=confirm, K=back, M=settings, Q/E=categories\n\n");

#ifdef __EMSCRIPTEN__
    // Run main loop at 60 FPS
    emscripten_set_main_loop(mainLoop, 60, 1);
#else
    // For non-Emscripten builds (testing), just render once
    Renderer::SelectScreen(0);
    Menu::RenderFrame();
    Renderer::EndFrame();
#endif

    return 0;
}

// =============================================================================
// Exported Functions for JavaScript
// =============================================================================

extern "C" {

/**
 * Change TV resolution (called from JavaScript)
 */
void setTvResolution(int resolution) {
    Renderer::SetTvResolution(resolution);
    printf("TV resolution changed to %dp\n", resolution);
}

} // extern "C"
