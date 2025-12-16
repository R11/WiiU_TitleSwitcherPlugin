/**
 * ASCII Preview Demo
 *
 * Demonstrates the menu rendering using the actual rendering code
 * from menu.cpp (via menu_render.cpp) with stub data.
 *
 * This ensures the preview matches what renders on actual hardware.
 */

#include "menu_render.h"
#include "stubs/renderer_stub.h"
#include "stubs/settings_stub.h"
#include "stubs/categories_stub.h"

#include <iostream>
#include <string>

void printUsage() {
    std::cout << "\n";
    std::cout << "=== Wii U Title Switcher - ASCII Preview ===\n";
    std::cout << "\n";
    std::cout << "This tool previews the menu layout using the ACTUAL rendering\n";
    std::cout << "code from menu.cpp, ensuring accuracy with hardware output.\n";
    std::cout << "\n";
    std::cout << "Usage: preview_demo [options]\n";
    std::cout << "  --no-color    Disable ANSI color codes\n";
    std::cout << "  --select N    Set selected title index (0-based)\n";
    std::cout << "  --scroll N    Set scroll offset\n";
    std::cout << "  --category N  Set category (0=All, 1=Favorites, 2=System, 3+=User)\n";
    std::cout << "  --numbers     Enable line numbers\n";
    std::cout << "  --no-favs     Hide favorite markers\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    bool useColor = true;
    int selectedIndex = 1;
    int scrollOffset = 0;
    int categoryIndex = 0;
    bool showNumbers = false;
    bool showFavorites = true;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-color") {
            useColor = false;
        } else if (arg == "--select" && i + 1 < argc) {
            selectedIndex = std::stoi(argv[++i]);
        } else if (arg == "--scroll" && i + 1 < argc) {
            scrollOffset = std::stoi(argv[++i]);
        } else if (arg == "--category" && i + 1 < argc) {
            categoryIndex = std::stoi(argv[++i]);
        } else if (arg == "--numbers") {
            showNumbers = true;
        } else if (arg == "--no-favs") {
            showFavorites = false;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    // Initialize stubs
    Settings::Init();
    Settings::Get().showNumbers = showNumbers;
    Settings::Get().showFavorites = showFavorites;

    Categories::Init();
    if (categoryIndex > 0) {
        Categories::SelectCategory(categoryIndex);
    }

    // Clamp selection to valid range
    int count = Categories::GetFilteredCount();
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= count && count > 0) selectedIndex = count - 1;

    // Initialize renderer
    Renderer::Init();

    // Set menu state
    MenuRender::SetSelection(selectedIndex, scrollOffset);

    std::cout << "\n=== Wii U GamePad Screen Preview ===\n";
    std::cout << "(Using actual menu.cpp rendering code)\n\n";

    // Render the frame
    uint32_t bgColor = Settings::Get().bgColor;
    Renderer::BeginFrame(bgColor);
    MenuRender::renderBrowseMode();

    // Get and print output
    std::string output = Renderer::GetFrameOutput(useColor);
    std::cout << output;

    Renderer::Shutdown();

    return 0;
}
