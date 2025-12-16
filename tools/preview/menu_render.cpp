/**
 * Menu Rendering - Extracted from src/menu/menu.cpp
 *
 * This file contains the rendering logic from menu.cpp, adapted for
 * desktop preview. It uses the stub implementations for dependencies.
 *
 * IMPORTANT: This file should be kept in sync with src/menu/menu.cpp
 * when the rendering logic changes.
 */

// Include stubs for desktop compilation
#include "stubs/renderer_stub.h"
#include "stubs/titles_stub.h"
#include "stubs/settings_stub.h"
#include "stubs/categories_stub.h"
#include "stubs/image_loader_stub.h"
#include "stubs/buttons_stub.h"
#include "stubs/text_input_stub.h"

#include <cstdio>
#include <cstring>

namespace MenuRender {

// =============================================================================
// Display Constants (from menu.h)
// =============================================================================

constexpr int VISIBLE_ROWS = 15;
constexpr int LIST_START_COL = 0;
constexpr int LIST_WIDTH = 30;
constexpr int DIVIDER_COL = 30;
constexpr int DETAILS_START_COL = 32;
constexpr int TITLE_NAME_WIDTH = 24;
constexpr int TITLE_NAME_WIDTH_NUM = 21;
constexpr int CATEGORY_ROW = 0;
constexpr int HEADER_ROW = 1;
constexpr int LIST_START_ROW = 2;
constexpr int FOOTER_ROW = 17;

// =============================================================================
// Rendering State (mirrors menu.cpp state)
// =============================================================================

int sSelectedIndex = 1;     // Currently selected title
int sScrollOffset = 0;      // First visible title

// =============================================================================
// State Access
// =============================================================================

void SetSelection(int index, int scroll) {
    sSelectedIndex = index;
    sScrollOffset = scroll;
}

int GetSelectedIndex() { return sSelectedIndex; }
int GetScrollOffset() { return sScrollOffset; }

// =============================================================================
// Helper: Clamp Selection (from menu.cpp)
// =============================================================================

void clampSelection() {
    int count = Categories::GetFilteredCount();

    if (sSelectedIndex < 0) sSelectedIndex = 0;
    if (sSelectedIndex >= count) sSelectedIndex = count > 0 ? count - 1 : 0;

    if (sSelectedIndex < sScrollOffset) {
        sScrollOffset = sSelectedIndex;
    }
    if (sSelectedIndex >= sScrollOffset + VISIBLE_ROWS) {
        sScrollOffset = sSelectedIndex - VISIBLE_ROWS + 1;
    }

    if (sScrollOffset < 0) sScrollOffset = 0;
    int maxScroll = count - VISIBLE_ROWS;
    if (maxScroll < 0) maxScroll = 0;
    if (sScrollOffset > maxScroll) sScrollOffset = maxScroll;
}

// =============================================================================
// Drawing Functions (from menu.cpp)
// =============================================================================

void drawCategoryBar() {
    char line[80];
    int col = 0;

    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    for (int i = 0; i < catCount && col < 60; i++) {
        if (!Categories::IsCategoryVisible(i)) continue;

        const char* name = Categories::GetCategoryName(i);

        if (i == currentCat) {
            snprintf(line, sizeof(line), "[%s] ", name);
        } else {
            snprintf(line, sizeof(line), " %s  ", name);
        }

        Renderer::DrawText(col, CATEGORY_ROW, line);
        col += strlen(line);
    }
}

void drawDivider() {
    for (int row = LIST_START_ROW; row < FOOTER_ROW; row++) {
        Renderer::DrawText(DIVIDER_COL, row, "|");
    }
}

void drawTitleList() {
    int count = Categories::GetFilteredCount();

    if (count == 0) {
        Renderer::DrawText(2, LIST_START_ROW + 2, "No titles in this category");
        return;
    }

    bool showNums = Settings::Get().showNumbers;
    int maxNameLen = showNums ? TITLE_NAME_WIDTH_NUM : TITLE_NAME_WIDTH;

    for (int i = 0; i < VISIBLE_ROWS && (sScrollOffset + i) < count; i++) {
        int idx = sScrollOffset + i;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);

        if (!title) continue;

        char line[48];
        char displayName[32];
        strncpy(displayName, title->name, maxNameLen);
        displayName[maxNameLen] = '\0';

        const char* cursor = (idx == sSelectedIndex) ? ">" : " ";

        bool showFavs = Settings::Get().showFavorites;
        const char* favMark = "";
        if (showFavs) {
            bool isFav = Settings::IsFavorite(title->titleId);
            favMark = isFav ? "* " : "  ";
        }

        if (showNums) {
            snprintf(line, sizeof(line), "%s%3d. %s%-*s",
                     cursor, idx + 1, favMark, maxNameLen, displayName);
        } else {
            snprintf(line, sizeof(line), "%s %s%-*s",
                     cursor, favMark, maxNameLen, displayName);
        }

        Renderer::DrawText(LIST_START_COL, LIST_START_ROW + i, line);
    }

    // Scroll indicators
    if (sScrollOffset > 0) {
        Renderer::DrawText(LIST_WIDTH - 4, LIST_START_ROW, "[UP]");
    }
    if (sScrollOffset + VISIBLE_ROWS < count) {
        Renderer::DrawText(LIST_WIDTH - 6, LIST_START_ROW + VISIBLE_ROWS - 1, "[DOWN]");
    }
}

void drawDetailsPanel() {
    int count = Categories::GetFilteredCount();
    if (count == 0 || sSelectedIndex < 0 || sSelectedIndex >= count) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
    if (!title) return;

    // Request icon loading
    ImageLoader::Request(title->titleId, ImageLoader::Priority::HIGH);

    // Title name
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW, title->name);

    // Draw icon
    constexpr int ICON_SIZE = 128;
    int screenWidth = Renderer::GetScreenWidth();
    int iconX = (screenWidth / 2) + 20;
    int iconY = Renderer::RowToPixelY(LIST_START_ROW + 1);

    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, ICON_SIZE, ICON_SIZE);
    } else {
        Renderer::DrawPlaceholder(iconX, iconY, ICON_SIZE, ICON_SIZE, 0x333333FF);
    }

    // Info after icon
    constexpr int INFO_START_ROW = LIST_START_ROW + 7;

    char idStr[32];
    snprintf(idStr, sizeof(idStr), "ID: %016llX",
             static_cast<unsigned long long>(title->titleId));
    Renderer::DrawText(DETAILS_START_COL, INFO_START_ROW, idStr);

    const char* favStatus = Settings::IsFavorite(title->titleId) ? "Yes" : "No";
    Renderer::DrawTextF(DETAILS_START_COL, INFO_START_ROW + 1, "Favorite: %s", favStatus);

    // Categories
    Renderer::DrawText(DETAILS_START_COL, INFO_START_ROW + 3, "Categories:");

    uint16_t catIds[Settings::MAX_CATEGORIES];
    int catCount = Settings::GetCategoriesForTitle(title->titleId, catIds, Settings::MAX_CATEGORIES);

    if (catCount == 0) {
        Renderer::DrawText(DETAILS_START_COL + 2, INFO_START_ROW + 4, "(none)");
    } else {
        int row = INFO_START_ROW + 4;
        for (int i = 0; i < catCount && row < FOOTER_ROW - 1; i++) {
            const Settings::Category* cat = Settings::GetCategory(catIds[i]);
            if (cat) {
                Renderer::DrawTextF(DETAILS_START_COL + 2, row, "- %s", cat->name);
                row++;
            }
        }
    }
}

void drawFooter() {
    int count = Categories::GetFilteredCount();

    char footer[100];
    snprintf(footer, sizeof(footer),
             "%s:Go %s:Close %s:Fav %s:Edit %s:Settings ZL/ZR:Cat [%d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             Buttons::Actions::FAVORITE.label,
             Buttons::Actions::EDIT.label,
             Buttons::Actions::SETTINGS.label,
             sSelectedIndex + 1,
             count);

    Renderer::DrawText(0, FOOTER_ROW, footer);
}

// =============================================================================
// Main Render Function
// =============================================================================

void renderBrowseMode() {
    drawCategoryBar();
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");
    drawDivider();
    drawTitleList();
    drawDetailsPanel();
    drawFooter();
}

void renderFrame(uint32_t bgColor) {
    Renderer::BeginFrame(bgColor);
    renderBrowseMode();
    Renderer::EndFrame();
}

} // namespace MenuRender
