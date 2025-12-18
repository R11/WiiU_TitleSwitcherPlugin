/**
 * Web Preview Main Entry Point
 *
 * Provides an interactive menu preview in the browser using WebAssembly.
 * Uses the same rendering code as the actual plugin but with mock data.
 * Renders to both TV and GamePad (DRC) screens simultaneously.
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

// Forward declarations from canvas_renderer.cpp
namespace Renderer {
    void SelectScreen(int screen);
    void SetTvResolution(int resolution);
}

namespace {

// =============================================================================
// Menu State
// =============================================================================

// List view states for each screen (they share selection state)
UI::ListView::State sListState;
UI::ListView::Config sDrcListConfig;
UI::ListView::Config sTvListConfig;

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
 * Render content to a specific screen
 */
void renderScreen(int screenId, const UI::ListView::Config& listConfig) {
    const Settings::PluginSettings& settings = Settings::Get();

    // Select the target screen
    Renderer::SelectScreen(screenId);

    // Begin frame with background color
    Renderer::BeginFrame(settings.bgColor);

    // Draw UI components
    drawCategoryBar();
    drawHeader();

    // Draw title list using ListView with screen-specific config
    UI::ListView::Render(sListState, listConfig, [&](int index, bool isSelected) {
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
}

/**
 * Main rendering function - renders to both screens
 */
void render() {
    // Render to DRC (GamePad)
    renderScreen(0, sDrcListConfig);

    // Render to TV
    renderScreen(1, sTvListConfig);

    // Push both framebuffers to canvases
    Renderer::EndFrame();
}

/**
 * Update list configs based on current screen dimensions
 */
void updateListConfigs() {
    // DRC config (854x480)
    Renderer::SelectScreen(0);
    sDrcListConfig.col = 0;
    sDrcListConfig.row = LIST_START_ROW;
    sDrcListConfig.width = Renderer::GetListWidth();
    sDrcListConfig.visibleRows = Renderer::GetVisibleRows();
    sDrcListConfig.showLineNumbers = Settings::Get().showNumbers;
    sDrcListConfig.showScrollIndicators = true;
    sDrcListConfig.canConfirm = true;
    sDrcListConfig.canCancel = false;
    sDrcListConfig.canFavorite = true;

    // TV config (variable resolution)
    Renderer::SelectScreen(1);
    sTvListConfig.col = 0;
    sTvListConfig.row = LIST_START_ROW;
    sTvListConfig.width = Renderer::GetListWidth();
    sTvListConfig.visibleRows = Renderer::GetVisibleRows();
    sTvListConfig.showLineNumbers = Settings::Get().showNumbers;
    sTvListConfig.showScrollIndicators = true;
    sTvListConfig.canConfirm = true;
    sTvListConfig.canCancel = false;
    sTvListConfig.canFavorite = true;

    // Reset to DRC for input handling
    Renderer::SelectScreen(0);
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
        sListState.SetItemCount(Categories::GetFilteredCount(), sDrcListConfig.visibleRows);
    }
    if (pressed & VPAD_BUTTON_ZR) {
        Categories::NextCategory();
        sListState.SetItemCount(Categories::GetFilteredCount(), sDrcListConfig.visibleRows);
    }

    // Handle list navigation (use DRC config for input)
    UI::ListView::HandleInput(sListState, pressed, sDrcListConfig);

    // Handle actions
    auto action = UI::ListView::GetAction(pressed, sDrcListConfig);
    switch (action) {
        case UI::ListView::Action::FAVORITE: {
            int idx = UI::ListView::GetSelectedIndex(sListState);
            if (idx >= 0) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);
                if (title) {
                    Settings::ToggleFavorite(title->titleId);
                    Categories::RefreshFilter();
                    sListState.SetItemCount(Categories::GetFilteredCount(), sDrcListConfig.visibleRows);
                }
            }
            break;
        }
        case UI::ListView::Action::CONFIRM: {
            // In web preview, just show a message
            const Titles::TitleInfo* title = Categories::GetFilteredTitle(sListState.selectedIndex);
            if (title) {
                printf("Selected title: %s\n", title->name);
            }
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
    printf("Wii U Title Switcher - Web Preview (Dual Screen)\n");
    printf("=================================================\n\n");

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

    // Configure list views for both screens
    updateListConfigs();

    // Initialize list state
    sListState.SetItemCount(Categories::GetFilteredCount(), sDrcListConfig.visibleRows);

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
 * Change TV resolution (called from JavaScript)
 */
void setTvResolution(int resolution) {
    Renderer::SetTvResolution(resolution);
    updateListConfigs();
    printf("TV resolution changed to %dp\n", resolution);
}

} // extern "C"
