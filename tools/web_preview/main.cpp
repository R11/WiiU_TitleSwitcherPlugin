/**
 * Web Preview Main Entry Point
 *
 * Provides an interactive menu preview in the browser using WebAssembly.
 * Uses the same rendering code as the actual plugin but with mock data.
 */

#include "render/renderer.h"
#include "storage/settings.h"
#include "titles/titles.h"
#include "menu/categories.h"
#include "ui/list_view.h"
#include "vpad/input.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdio>
#include <cstring>

// Forward declarations from web_input.cpp
extern "C" {
    void onKeyDown(int keyCode);
    void onKeyUp(int keyCode);
}

namespace {

// =============================================================================
// Menu State
// =============================================================================

// List view state
UI::ListView::State sListState;
UI::ListView::Config sListConfig;

// Currently selected category
int sCurrentCategory = 0;

// Layout constants (matching menu.cpp)
constexpr int CATEGORY_ROW = 0;
constexpr int HEADER_ROW = 1;
constexpr int LIST_START_ROW = 2;

// =============================================================================
// Rendering Functions
// =============================================================================

/**
 * Draw the category bar at the top of the screen
 */
void drawCategoryBar() {
    const Settings::PluginSettings& settings = Settings::Get();
    int col = 0;

    for (int i = 0; i < Categories::GetTotalCategoryCount(); i++) {
        if (!Categories::IsCategoryVisible(i)) continue;

        const char* name = Categories::GetCategoryName(i);
        if (!name) continue;

        // Format: [CategoryName]
        char buf[64];
        snprintf(buf, sizeof(buf), "[%s]", name);

        uint32_t color;
        if (i == Categories::GetCurrentCategoryIndex()) {
            color = settings.highlightedTitleColor;
        } else {
            color = settings.categoryColor;
        }

        Renderer::DrawText(col, CATEGORY_ROW, buf, color);
        col += (int)strlen(buf) + 1;
    }

    // Draw help text on right side
    int helpCol = Renderer::GetGridWidth() - 20;
    Renderer::DrawText(helpCol, CATEGORY_ROW, "Q/E: Categories", 0x6C7086FF);
}

/**
 * Draw the header/divider row
 */
void drawHeader() {
    const Settings::PluginSettings& settings = Settings::Get();
    int width = Renderer::GetGridWidth();

    // Draw title section header
    Renderer::DrawText(0, HEADER_ROW, "Titles", settings.headerColor);

    // Draw divider
    int dividerCol = Renderer::GetDividerCol();
    for (int i = 0; i < width; i++) {
        if (i == dividerCol) {
            Renderer::DrawText(i, HEADER_ROW, "|", 0x45475AFF);
        }
    }

    // Draw details header
    int detailsCol = Renderer::GetDetailsPanelCol();
    Renderer::DrawText(detailsCol, HEADER_ROW, "Details", settings.headerColor);
}

/**
 * Draw the details panel for the selected title
 */
void drawDetailsPanel() {
    const Settings::PluginSettings& settings = Settings::Get();
    int col = Renderer::GetDetailsPanelCol();
    int row = LIST_START_ROW;

    int selectedIdx = UI::ListView::GetSelectedIndex(sListState);
    if (selectedIdx < 0) {
        Renderer::DrawText(col, row, "No title selected", 0x6C7086FF);
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
    if (!title) return;

    // Full title name
    Renderer::DrawText(col, row++, title->name, settings.titleColor);

    // Separator
    row++;

    // Title ID
    char buf[64];
    snprintf(buf, sizeof(buf), "ID: %016llX", (unsigned long long)title->titleId);
    Renderer::DrawText(col, row++, buf, 0xA6ADC8FF);

    // Product code
    if (title->productCode[0]) {
        snprintf(buf, sizeof(buf), "Code: %s", title->productCode);
        Renderer::DrawText(col, row++, buf, 0xA6ADC8FF);
    }

    // Favorite status
    row++;
    if (Settings::IsFavorite(title->titleId)) {
        Renderer::DrawText(col, row++, "* Favorited", settings.favoriteColor);
    }

    // Controls hint
    row = Renderer::GetFooterRow() - 3;
    Renderer::DrawText(col, row++, "Controls:", 0x6C7086FF);
    Renderer::DrawText(col, row++, "Enter: Select", 0x6C7086FF);
    Renderer::DrawText(col, row++, "F: Toggle Favorite", 0x6C7086FF);
}

/**
 * Draw the footer with controls and count
 */
void drawFooter() {
    const Settings::PluginSettings& settings = Settings::Get();
    int row = Renderer::GetFooterRow();

    // Left side: controls
    Renderer::DrawText(0, row, "Enter:Launch  Esc:Close  F:Fav  +:Settings", 0xA6ADC8FF);

    // Right side: count
    char buf[32];
    int count = Categories::GetFilteredCount();
    int selected = sListState.selectedIndex + 1;
    snprintf(buf, sizeof(buf), "[%d/%d]", selected, count);

    int countCol = Renderer::GetGridWidth() - (int)strlen(buf) - 1;
    Renderer::DrawText(countCol, row, buf, 0xA6ADC8FF);
}

/**
 * Main rendering function
 */
void render() {
    const Settings::PluginSettings& settings = Settings::Get();

    // Begin frame with background color
    Renderer::BeginFrame(settings.bgColor);

    // Draw UI components
    drawCategoryBar();
    drawHeader();

    // Draw title list using ListView
    UI::ListView::Render(sListState, sListConfig, [&](int index, bool isSelected) {
        UI::ListView::ItemView view;

        const Titles::TitleInfo* title = Categories::GetFilteredTitle(index);
        if (!title) {
            view.text = "???";
            return view;
        }

        view.text = title->name;
        bool isFavorite = Settings::IsFavorite(title->titleId);

        // Set prefix
        if (isSelected) {
            view.prefix = "> ";
        } else if (isFavorite && settings.showFavorites) {
            view.prefix = "* ";
        } else {
            view.prefix = "  ";
        }

        // Set colors
        if (isSelected) {
            view.textColor = settings.highlightedTitleColor;
            view.prefixColor = settings.highlightedTitleColor;
        } else if (isFavorite) {
            view.textColor = settings.favoriteColor;
            view.prefixColor = settings.favoriteColor;
        } else {
            view.textColor = settings.titleColor;
            view.prefixColor = settings.titleColor;
        }

        return view;
    });

    drawDetailsPanel();
    drawFooter();

    // End frame - pushes to canvas
    Renderer::EndFrame();
}

/**
 * Handle input and update state
 */
void handleInput() {
    VPADStatus status;
    VPADReadError error;

    if (VPADRead(VPAD_CHAN_0, &status, 1, &error) <= 0) {
        return;
    }

    uint32_t pressed = status.trigger;

    // Category navigation
    if (pressed & VPAD_BUTTON_ZL) {
        Categories::PreviousCategory();
        sListState.SetItemCount(Categories::GetFilteredCount(), sListConfig.visibleRows);
    }
    if (pressed & VPAD_BUTTON_ZR) {
        Categories::NextCategory();
        sListState.SetItemCount(Categories::GetFilteredCount(), sListConfig.visibleRows);
    }

    // Handle list navigation
    UI::ListView::HandleInput(sListState, pressed, sListConfig);

    // Handle actions
    auto action = UI::ListView::GetAction(pressed, sListConfig);
    switch (action) {
        case UI::ListView::Action::FAVORITE: {
            int idx = UI::ListView::GetSelectedIndex(sListState);
            if (idx >= 0) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);
                if (title) {
                    Settings::ToggleFavorite(title->titleId);
                    Categories::RefreshFilter();
                    sListState.SetItemCount(Categories::GetFilteredCount(), sListConfig.visibleRows);
                }
            }
            break;
        }
        case UI::ListView::Action::CONFIRM: {
            // In web preview, just show a message
            printf("Selected title: %s\n",
                   Categories::GetFilteredTitle(sListState.selectedIndex)->name);
            break;
        }
        default:
            break;
    }
}

/**
 * Main loop function called by Emscripten
 */
void mainLoop() {
    handleInput();
    render();
}

} // anonymous namespace

// =============================================================================
// Entry Point
// =============================================================================

int main() {
    printf("Wii U Title Switcher - Web Preview\n");
    printf("===================================\n\n");

    // Initialize renderer
    if (!Renderer::Init()) {
        printf("ERROR: Failed to initialize renderer\n");
        return 1;
    }

    // Initialize settings and titles
    Settings::Init();
    Settings::Load();
    Titles::Load();

    // Initialize categories
    Categories::Init();

    // Configure list view
    sListConfig.col = 0;
    sListConfig.row = LIST_START_ROW;
    sListConfig.width = Renderer::GetListWidth();
    sListConfig.visibleRows = Renderer::GetVisibleRows();
    sListConfig.showLineNumbers = Settings::Get().showNumbers;
    sListConfig.showScrollIndicators = true;
    sListConfig.canConfirm = true;
    sListConfig.canCancel = false;  // No cancel in preview
    sListConfig.canFavorite = true;

    // Initialize list state
    sListState.SetItemCount(Categories::GetFilteredCount(), sListConfig.visibleRows);

    printf("Loaded %d titles\n", Titles::GetCount());
    printf("Use arrow keys to navigate, Q/E for categories\n\n");

#ifdef __EMSCRIPTEN__
    // Run main loop at 60 FPS
    emscripten_set_main_loop(mainLoop, 60, 1);
#else
    // For non-Emscripten builds (testing), just render once
    render();
#endif

    return 0;
}

// =============================================================================
// Exported Functions for JavaScript
// =============================================================================

extern "C" {

/**
 * Change screen type (called from JavaScript)
 * TODO: Implement actual screen type switching with different resolutions
 */
void setScreenType(int type) {
    // For now, just acknowledge the request
    // Full implementation would require resizing the framebuffer
    printf("Screen type requested: %d\n", type);
}

} // extern "C"
