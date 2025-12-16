/**
 * ASCII Preview Demo
 *
 * Demonstrates the ASCII renderer with sample Wii U title menu data.
 * Mimics the actual menu layout from the TitleSwitcher plugin.
 */

#include "ascii_renderer.h"
#include <iostream>
#include <vector>
#include <string>

// Layout constants (from menu.h)
constexpr int VISIBLE_ROWS = 15;
constexpr int LIST_START_COL = 0;
constexpr int LIST_WIDTH = 30;
constexpr int DIVIDER_COL = 30;
constexpr int DETAILS_START_COL = 32;
constexpr int CATEGORY_ROW = 0;
constexpr int HEADER_ROW = 1;
constexpr int LIST_START_ROW = 2;
constexpr int FOOTER_ROW = 17;

// Colors (RGBA format: 0xRRGGBBAA)
constexpr uint32_t COLOR_WHITE     = 0xFFFFFFFF;
constexpr uint32_t COLOR_CYAN      = 0x00FFFFFF;
constexpr uint32_t COLOR_YELLOW    = 0xFFFF00FF;
constexpr uint32_t COLOR_GREEN     = 0x00FF00FF;
constexpr uint32_t COLOR_GRAY      = 0x808080FF;
constexpr uint32_t COLOR_BG        = 0x000000FF;
constexpr uint32_t COLOR_ICON_PLACEHOLDER = 0x404040FF;

// Sample title data
struct Title {
    std::string shortName;
    std::string fullName;
    std::string titleId;
    std::string region;
    bool favorite;
    std::vector<std::string> categories;
};

// Sample categories
const std::vector<std::string> SAMPLE_CATEGORIES = {
    "All", "Favorites", "Games", "Apps", "Demos"
};

// Sample titles (mimicking real Wii U games)
const std::vector<Title> SAMPLE_TITLES = {
    {"Super Mario 3D World", "Super Mario 3D World", "0005000010145D00", "USA", true, {"Games"}},
    {"Mario Kart 8", "Mario Kart 8", "000500001010EC00", "USA", true, {"Games"}},
    {"Splatoon", "Splatoon", "0005000010176900", "USA", false, {"Games"}},
    {"Super Smash Bros.", "Super Smash Bros. for Wii U", "0005000010145000", "USA", true, {"Games"}},
    {"Zelda: Breath Wild", "The Legend of Zelda: Breath of the Wild", "00050000101C9400", "USA", false, {"Games"}},
    {"Bayonetta 2", "Bayonetta 2", "0005000010172600", "USA", false, {"Games"}},
    {"Pikmin 3", "Pikmin 3", "0005000010145200", "USA", false, {"Games"}},
    {"Xenoblade X", "Xenoblade Chronicles X", "0005000010116100", "USA", false, {"Games"}},
    {"Hyrule Warriors", "Hyrule Warriors", "0005000010143500", "USA", false, {"Games"}},
    {"Donkey Kong TF", "Donkey Kong Country: Tropical Freeze", "0005000010138300", "USA", false, {"Games"}},
    {"Captain Toad", "Captain Toad: Treasure Tracker", "0005000010175B00", "USA", false, {"Games"}},
    {"Yoshi's Woolly", "Yoshi's Woolly World", "0005000010184D00", "USA", false, {"Games"}},
    {"Star Fox Zero", "Star Fox Zero", "0005000010185300", "USA", false, {"Games"}},
    {"NES Remix Pack", "NES Remix Pack", "0005000010132200", "EUR", false, {"Games"}},
    {"Mii Verse", "Mii Verse", "000500301001500A", "USA", false, {"Apps"}},
    {"eShop", "Nintendo eShop", "000500301001200A", "USA", false, {"Apps"}},
    {"Internet Browser", "Internet Browser", "000500301001000A", "USA", false, {"Apps"}},
};

int gSelectedIndex = 1;  // Currently selected title
int gScrollOffset = 0;   // Scroll position
int gSelectedCategory = 0; // Current category tab

void drawCategoryBar() {
    std::string line;
    for (size_t i = 0; i < SAMPLE_CATEGORIES.size(); i++) {
        if (i == (size_t)gSelectedCategory) {
            line += "[" + SAMPLE_CATEGORIES[i] + "] ";
        } else {
            line += " " + SAMPLE_CATEGORIES[i] + "  ";
        }
    }

    // Pad to fit, add navigation hint at end
    while (line.length() < 85) line += " ";
    line += "[ZL/ZR]";

    AsciiRenderer::DrawText(0, CATEGORY_ROW, line.c_str(), COLOR_WHITE);
}

void drawDivider() {
    // Horizontal divider under category bar
    std::string divider(100, '-');
    AsciiRenderer::DrawText(0, HEADER_ROW, divider.c_str(), COLOR_GRAY);

    // Vertical divider between list and details
    for (int row = LIST_START_ROW; row < FOOTER_ROW; row++) {
        AsciiRenderer::DrawText(DIVIDER_COL, row, "|", COLOR_GRAY);
    }
}

void drawTitleList() {
    int count = SAMPLE_TITLES.size();

    // Draw scroll indicator (up)
    if (gScrollOffset > 0) {
        AsciiRenderer::DrawText(LIST_WIDTH - 4, LIST_START_ROW, "[UP]", COLOR_CYAN);
    }

    // Draw visible titles
    for (int i = 0; i < VISIBLE_ROWS && (gScrollOffset + i) < count; i++) {
        const Title& title = SAMPLE_TITLES[gScrollOffset + i];
        int displayIndex = gScrollOffset + i;

        // Build line: "  1. * Title Name" or "> 2.   Title Name"
        char line[64];
        const char* prefix = (displayIndex == gSelectedIndex) ? ">" : " ";
        const char* favMark = title.favorite ? "*" : " ";

        snprintf(line, sizeof(line), "%s%2d. %s %-20s",
                 prefix, displayIndex + 1, favMark, title.shortName.c_str());

        // Truncate to fit list width
        line[LIST_WIDTH - 1] = '\0';

        uint32_t color = (displayIndex == gSelectedIndex) ? COLOR_CYAN : COLOR_WHITE;
        AsciiRenderer::DrawText(LIST_START_COL, LIST_START_ROW + i, line, color);
    }

    // Draw scroll indicator (down)
    if (gScrollOffset + VISIBLE_ROWS < count) {
        AsciiRenderer::DrawText(LIST_WIDTH - 6, LIST_START_ROW + VISIBLE_ROWS - 1, "[DOWN]", COLOR_CYAN);
    }
}

void drawDetailsPanel() {
    if (gSelectedIndex < 0 || gSelectedIndex >= (int)SAMPLE_TITLES.size()) {
        return;
    }

    const Title& title = SAMPLE_TITLES[gSelectedIndex];

    // Full title name
    AsciiRenderer::DrawText(DETAILS_START_COL, LIST_START_ROW, title.fullName.c_str(), COLOR_WHITE);

    // Separator line
    AsciiRenderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 1,
                            "----------------------------------------", COLOR_GRAY);

    // Icon placeholder (simulated as a colored region)
    // In the real menu this would be at pixel coordinates
    int iconX = AsciiRenderer::ColToPixelX(DETAILS_START_COL);
    int iconY = AsciiRenderer::RowToPixelY(LIST_START_ROW + 2);
    AsciiRenderer::DrawPlaceholder(iconX, iconY, 64, 64, COLOR_ICON_PLACEHOLDER);

    // Title info (positioned after icon area)
    int infoRow = LIST_START_ROW + 5;
    AsciiRenderer::DrawTextF(DETAILS_START_COL, infoRow, COLOR_WHITE,
                             "Title ID: %s", title.titleId.c_str());
    AsciiRenderer::DrawTextF(DETAILS_START_COL, infoRow + 1, COLOR_WHITE,
                             "Region: %s", title.region.c_str());
    AsciiRenderer::DrawTextF(DETAILS_START_COL, infoRow + 2, COLOR_WHITE,
                             "Favorite: %s", title.favorite ? "Yes" : "No");

    // Categories
    AsciiRenderer::DrawText(DETAILS_START_COL, infoRow + 4, "Categories:", COLOR_WHITE);
    if (title.categories.empty()) {
        AsciiRenderer::DrawText(DETAILS_START_COL + 2, infoRow + 5, "(none)", COLOR_GRAY);
    } else {
        int catRow = infoRow + 5;
        for (const auto& cat : title.categories) {
            AsciiRenderer::DrawTextF(DETAILS_START_COL + 2, catRow, COLOR_WHITE, "- %s", cat.c_str());
            catRow++;
        }
    }

    // Action hints
    AsciiRenderer::DrawText(DETAILS_START_COL, FOOTER_ROW - 2, "X: Edit Title", COLOR_YELLOW);
}

void drawFooter() {
    char footer[128];
    snprintf(footer, sizeof(footer),
             "A:Launch B:Close Y:Fav X:Edit +:Settings                    [%d/%d]",
             gSelectedIndex + 1, (int)SAMPLE_TITLES.size());

    AsciiRenderer::DrawText(0, FOOTER_ROW, footer, COLOR_WHITE);
}

void renderMenuFrame() {
    AsciiRenderer::BeginFrame(COLOR_BG);

    drawCategoryBar();
    drawDivider();
    drawTitleList();
    drawDetailsPanel();
    drawFooter();

    AsciiRenderer::EndFrame(true, true);
}

void printUsage() {
    std::cout << "\n";
    std::cout << "=== Wii U Title Switcher - ASCII Preview ===\n";
    std::cout << "\n";
    std::cout << "This tool previews the menu layout as it would appear on the GamePad.\n";
    std::cout << "\n";
    std::cout << "Usage: preview_demo [options]\n";
    std::cout << "  --no-color    Disable ANSI color codes\n";
    std::cout << "  --ascii       Use plain ASCII (no Unicode blocks)\n";
    std::cout << "  --tv          Show TV screen layout instead of DRC\n";
    std::cout << "  --select N    Set selected title index (0-based)\n";
    std::cout << "  --scroll N    Set scroll offset\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    bool useColor = true;
    bool useUnicode = true;
    AsciiRenderer::Screen screen = AsciiRenderer::Screen::DRC;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-color") {
            useColor = false;
        } else if (arg == "--ascii") {
            useUnicode = false;
        } else if (arg == "--tv") {
            screen = AsciiRenderer::Screen::TV;
        } else if (arg == "--select" && i + 1 < argc) {
            gSelectedIndex = std::stoi(argv[++i]);
        } else if (arg == "--scroll" && i + 1 < argc) {
            gScrollOffset = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
    }

    // Clamp values
    if (gSelectedIndex < 0) gSelectedIndex = 0;
    if (gSelectedIndex >= (int)SAMPLE_TITLES.size()) {
        gSelectedIndex = SAMPLE_TITLES.size() - 1;
    }
    if (gScrollOffset < 0) gScrollOffset = 0;

    // Initialize and render
    AsciiRenderer::Init(screen);

    std::cout << "\n=== Wii U GamePad Screen Preview ===\n\n";

    // Render the frame
    AsciiRenderer::BeginFrame(COLOR_BG);
    drawCategoryBar();
    drawDivider();
    drawTitleList();
    drawDetailsPanel();
    drawFooter();

    // Get and print output
    std::string output = AsciiRenderer::GetFrameOutput(useColor, useUnicode);
    std::cout << output;

    AsciiRenderer::Shutdown();

    return 0;
}
