/**
 * ASCII Preview Demo
 *
 * Demonstrates the menu rendering using the actual rendering code
 * from menu.cpp (via menu_render.cpp) with stub data.
 *
 * Supports multiple screen types:
 * - DRC (GamePad): 854x480, 16:9
 * - TV 1080p: 1920x1080, 16:9
 * - TV 720p: 1280x720, 16:9
 * - TV 480p: 640x480, 4:3
 *
 * This ensures the preview matches what renders on actual hardware.
 */

#include "menu_render.h"
#include "stubs/renderer_stub.h"
#include "stubs/settings_stub.h"
#include "stubs/categories_stub.h"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

void printUsage() {
    std::cout << "\n";
    std::cout << "=== Wii U Title Switcher - ASCII Preview ===\n";
    std::cout << "\n";
    std::cout << "This tool previews the menu layout using the ACTUAL rendering\n";
    std::cout << "code from menu.cpp, ensuring accuracy with hardware output.\n";
    std::cout << "\n";
    std::cout << "Usage: preview_demo [options]\n";
    std::cout << "\n";
    std::cout << "Screen options:\n";
    std::cout << "  --screen TYPE   Set screen type:\n";
    std::cout << "                    drc     - GamePad (854x480, 16:9) [default]\n";
    std::cout << "                    tv1080  - TV 1080p (1920x1080, 16:9)\n";
    std::cout << "                    tv720   - TV 720p (1280x720, 16:9)\n";
    std::cout << "                    tv480   - TV 480p (640x480, 4:3)\n";
    std::cout << "\n";
    std::cout << "Display options:\n";
    std::cout << "  --no-color      Disable ANSI color codes\n";
    std::cout << "  --compact       Scale to 78 columns (fits 80-col terminal)\n";
    std::cout << "  --width N       Set compact width (default: 78)\n";
    std::cout << "  --raw           Output raw text buffer (no borders/colors)\n";
    std::cout << "  --numbers       Enable line numbers\n";
    std::cout << "  --no-favs       Hide favorite markers\n";
    std::cout << "\n";
    std::cout << "Navigation options:\n";
    std::cout << "  --select N      Set selected title index (0-based)\n";
    std::cout << "  --scroll N      Set scroll offset\n";
    std::cout << "  --category N    Set category (0=All, 1=Favorites, 2=System, 3+=User)\n";
    std::cout << "\n";
    std::cout << "Snapshot options:\n";
    std::cout << "  --update-snapshots   Regenerate all snapshot files\n";
    std::cout << "  --verify-snapshots   Compare current output against snapshots\n";
    std::cout << "\n";
}

// =============================================================================
// Snapshot Testing
// =============================================================================

struct SnapshotConfig {
    const char* filename;
    int selection;
    int scroll;
    int category;
    bool showNumbers;
    bool showFavorites;
};

const SnapshotConfig SNAPSHOTS[] = {
    {"browse_default.txt",       1,  0, 0, false, true},
    {"browse_scrolled.txt",     10,  5, 0, false, true},
    {"browse_at_bottom.txt",    19, 10, 0, false, true},
    {"browse_favorites.txt",     0,  0, 1, false, true},
    {"browse_system.txt",        0,  0, 2, false, true},
    {"browse_games.txt",         0,  0, 3, false, true},
    {"browse_with_numbers.txt",  1,  0, 0, true,  true},
    {"browse_no_favorites.txt",  1,  0, 0, false, false},
};

const int SNAPSHOT_COUNT = sizeof(SNAPSHOTS) / sizeof(SNAPSHOTS[0]);
const char* SNAPSHOT_DIR = "snapshots";

// All screen types to include in snapshots
struct ScreenInfo {
    Renderer::ScreenType type;
    const char* name;
};

const ScreenInfo SCREEN_TYPES[] = {
    {Renderer::ScreenType::DRC,      "DRC (854x480)"},
    {Renderer::ScreenType::TV_1080P, "TV 1080p (1920x1080)"},
    {Renderer::ScreenType::TV_720P,  "TV 720p (1280x720)"},
    {Renderer::ScreenType::TV_480P,  "TV 480p (640x480)"},
};

const int SCREEN_TYPE_COUNT = sizeof(SCREEN_TYPES) / sizeof(SCREEN_TYPES[0]);

std::string renderForScreen(const SnapshotConfig& config, Renderer::ScreenType screenType) {
    // Reset state
    Settings::Init();
    Settings::Get().showNumbers = config.showNumbers;
    Settings::Get().showFavorites = config.showFavorites;

    Categories::Init();
    if (config.category > 0) {
        Categories::SelectCategory(config.category);
    }

    // Clamp selection
    int count = Categories::GetFilteredCount();
    int sel = config.selection;
    if (sel >= count && count > 0) sel = count - 1;

    Renderer::SetScreenType(screenType);
    Renderer::Init();
    MenuRender::SetSelection(sel, config.scroll);

    Renderer::BeginFrame(Settings::Get().bgColor);
    MenuRender::renderBrowseMode();

    std::string output = Renderer::GetTrimmedOutput();
    Renderer::Shutdown();

    return output;
}

std::string renderSnapshot(const SnapshotConfig& config) {
    std::string output;

    for (int i = 0; i < SCREEN_TYPE_COUNT; i++) {
        const auto& screen = SCREEN_TYPES[i];

        // Add separator header
        output += "================================================================================\n";
        output += "  " + std::string(screen.name) + "\n";
        output += "================================================================================\n\n";

        // Render for this screen type
        output += renderForScreen(config, screen.type);

        // Add spacing between screens (except after last)
        if (i < SCREEN_TYPE_COUNT - 1) {
            output += "\n\n";
        }
    }

    return output;
}

int updateSnapshots() {
    namespace fs = std::filesystem;

    // Create snapshots directory if it doesn't exist
    if (!fs::exists(SNAPSHOT_DIR)) {
        fs::create_directory(SNAPSHOT_DIR);
    }

    std::cout << "Updating " << SNAPSHOT_COUNT << " snapshots...\n";

    for (int i = 0; i < SNAPSHOT_COUNT; i++) {
        const auto& config = SNAPSHOTS[i];
        std::string output = renderSnapshot(config);

        std::string path = std::string(SNAPSHOT_DIR) + "/" + config.filename;
        std::ofstream file(path);
        if (file) {
            file << output;
            std::cout << "  Updated: " << config.filename << "\n";
        } else {
            std::cerr << "  FAILED:  " << config.filename << "\n";
            return 1;
        }
    }

    std::cout << "Done.\n";
    return 0;
}

int verifySnapshots() {
    namespace fs = std::filesystem;

    if (!fs::exists(SNAPSHOT_DIR)) {
        std::cerr << "Snapshot directory not found. Run --update-snapshots first.\n";
        return 1;
    }

    std::cout << "Verifying " << SNAPSHOT_COUNT << " snapshots...\n";

    int passed = 0;
    int failed = 0;

    for (int i = 0; i < SNAPSHOT_COUNT; i++) {
        const auto& config = SNAPSHOTS[i];
        std::string current = renderSnapshot(config);

        std::string path = std::string(SNAPSHOT_DIR) + "/" + config.filename;
        std::ifstream file(path);
        if (!file) {
            std::cerr << "  MISSING: " << config.filename << "\n";
            failed++;
            continue;
        }

        std::string expected((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

        if (current == expected) {
            std::cout << "  PASS:    " << config.filename << "\n";
            passed++;
        } else {
            std::cerr << "  FAIL:    " << config.filename << "\n";
            failed++;
        }
    }

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return (failed > 0) ? 1 : 0;
}

Renderer::ScreenType parseScreenType(const std::string& arg) {
    if (arg == "drc" || arg == "gamepad") {
        return Renderer::ScreenType::DRC;
    } else if (arg == "tv1080" || arg == "1080p" || arg == "1080") {
        return Renderer::ScreenType::TV_1080P;
    } else if (arg == "tv720" || arg == "720p" || arg == "720") {
        return Renderer::ScreenType::TV_720P;
    } else if (arg == "tv480" || arg == "480p" || arg == "480") {
        return Renderer::ScreenType::TV_480P;
    }
    return Renderer::ScreenType::DRC;  // Default
}

enum class OutputMode { Full, Compact, Raw };

int main(int argc, char* argv[]) {
    bool useColor = true;
    int selectedIndex = 1;
    int scrollOffset = 0;
    int categoryIndex = 0;
    bool showNumbers = false;
    bool showFavorites = true;
    Renderer::ScreenType screenType = Renderer::ScreenType::DRC;
    OutputMode outputMode = OutputMode::Full;
    int compactWidth = 78;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--update-snapshots") {
            return updateSnapshots();
        } else if (arg == "--verify-snapshots") {
            return verifySnapshots();
        } else if (arg == "--no-color") {
            useColor = false;
        } else if (arg == "--compact") {
            outputMode = OutputMode::Compact;
        } else if (arg == "--raw") {
            outputMode = OutputMode::Raw;
        } else if (arg == "--width" && i + 1 < argc) {
            compactWidth = std::stoi(argv[++i]);
            outputMode = OutputMode::Compact;
        } else if (arg == "--screen" && i + 1 < argc) {
            screenType = parseScreenType(argv[++i]);
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

    // Set screen type BEFORE initializing renderer
    Renderer::SetScreenType(screenType);

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

    if (outputMode != OutputMode::Raw) {
        std::cout << "\n=== Wii U Menu Preview ===\n";
        std::cout << "(Using actual menu.cpp rendering code)\n\n";
    }

    // Render the frame
    uint32_t bgColor = Settings::Get().bgColor;
    Renderer::BeginFrame(bgColor);
    MenuRender::renderBrowseMode();

    // Get and print output based on mode
    std::string output;
    switch (outputMode) {
        case OutputMode::Full:
            output = Renderer::GetFrameOutput(useColor);
            break;
        case OutputMode::Compact:
            output = Renderer::GetCompactOutput(compactWidth);
            break;
        case OutputMode::Raw:
            output = Renderer::GetRawText();
            break;
    }
    std::cout << output;

    Renderer::Shutdown();

    return 0;
}
