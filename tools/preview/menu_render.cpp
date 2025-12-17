/**
 * Menu Rendering - Extracted from src/menu/menu.cpp
 *
 * This file contains the rendering logic from menu.cpp, adapted for
 * desktop preview. It uses the stub implementations for dependencies.
 *
 * IMPORTANT: This file should be kept in sync with src/menu/menu.cpp
 * when the rendering logic changes.
 *
 * LAYOUT ADAPTATION:
 * Layout is calculated dynamically based on screen size using Renderer
 * helper functions. This allows the same rendering code to work on:
 * - DRC (GamePad): 854x480, 100x18 chars
 * - TV 1080p: 1920x1080, 160x36 chars
 * - TV 720p: 1280x720, 120x27 chars
 * - TV 480p (4:3): 640x480, 80x20 chars
 */

// Include stubs for desktop compilation
#include "stubs/renderer_stub.h"
#include "stubs/titles_stub.h"
#include "stubs/settings_stub.h"
#include "stubs/categories_stub.h"
#include "stubs/image_loader_stub.h"
#include "stubs/buttons_stub.h"
#include "stubs/text_input_stub.h"
#include "stubs/title_presets_stub.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace MenuRender {

// =============================================================================
// Fixed Layout Constants
// =============================================================================

// Row positions (fixed across all screen sizes - matches menu.h)
constexpr int CATEGORY_ROW = 0;
constexpr int HEADER_ROW = 1;
constexpr int LIST_START_ROW = 2;
constexpr int LIST_START_COL = 0;

// All column/width values use dynamic Renderer:: functions to support
// different screen sizes. See Renderer::GetDividerCol(), GetDetailsPanelCol(), etc.

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
    int visibleRows = Renderer::GetVisibleRows();

    if (sSelectedIndex < 0) sSelectedIndex = 0;
    if (sSelectedIndex >= count) sSelectedIndex = count > 0 ? count - 1 : 0;

    if (sSelectedIndex < sScrollOffset) {
        sScrollOffset = sSelectedIndex;
    }
    if (sSelectedIndex >= sScrollOffset + visibleRows) {
        sScrollOffset = sSelectedIndex - visibleRows + 1;
    }

    if (sScrollOffset < 0) sScrollOffset = 0;
    int maxScroll = count - visibleRows;
    if (maxScroll < 0) maxScroll = 0;
    if (sScrollOffset > maxScroll) sScrollOffset = maxScroll;
}

// =============================================================================
// Drawing Functions (from menu.cpp)
// =============================================================================

void drawCategoryBar() {
    char line[80];
    int col = 0;
    int maxCol = Renderer::GetGridWidth() - 10;  // Leave space for nav hint

    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    for (int i = 0; i < catCount && col < maxCol; i++) {
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
    int dividerCol = Renderer::GetDividerCol();
    int footerRow = Renderer::GetFooterRow();

    for (int row = LIST_START_ROW; row < footerRow; row++) {
        Renderer::DrawText(dividerCol, row, "|");
    }
}

void drawTitleList() {
    int count = Categories::GetFilteredCount();
    int visibleRows = Renderer::GetVisibleRows();
    int dividerCol = Renderer::GetDividerCol();

    if (count == 0) {
        Renderer::DrawText(2, LIST_START_ROW + 2, "No titles in this category");
        return;
    }

    bool showNums = Settings::Get().showNumbers;
    int maxNameLen = Renderer::GetTitleNameWidth(showNums);

    for (int i = 0; i < visibleRows && (sScrollOffset + i) < count; i++) {
        int idx = sScrollOffset + i;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);

        if (!title) continue;

        char line[128];
        char displayName[64];
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

        // Truncate line to fit before divider
        if ((int)strlen(line) >= dividerCol) {
            line[dividerCol - 1] = '\0';
        }

        Renderer::DrawText(LIST_START_COL, LIST_START_ROW + i, line);
    }

    // Scroll indicators
    if (sScrollOffset > 0) {
        Renderer::DrawText(dividerCol - 5, LIST_START_ROW, "[UP]");
    }
    if (sScrollOffset + visibleRows < count) {
        Renderer::DrawText(dividerCol - 7, LIST_START_ROW + visibleRows - 1, "[DOWN]");
    }
}

void drawDetailsPanel() {
    int count = Categories::GetFilteredCount();
    if (count == 0 || sSelectedIndex < 0 || sSelectedIndex >= count) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
    if (!title) return;

    int detailsCol = Renderer::GetDetailsPanelCol();
    int footerRow = Renderer::GetFooterRow();

    // Request icon loading
    ImageLoader::Request(title->titleId, ImageLoader::Priority::HIGH);

    // Title name (truncate to fit panel width)
    int panelWidth = Renderer::GetGridWidth() - detailsCol - 1;
    char titleName[128];
    strncpy(titleName, title->name, std::min(panelWidth, (int)sizeof(titleName) - 1));
    titleName[std::min(panelWidth, (int)sizeof(titleName) - 1)] = '\0';
    Renderer::DrawText(detailsCol, LIST_START_ROW, titleName);

    // Draw icon below title - matches src/menu/menu.cpp logic
    // Position using screen percentages to avoid character size mismatch
    int iconSize = Renderer::GetIconSize();
    int screenWidth = Renderer::GetScreenWidth();
    int screenHeight = Renderer::GetScreenHeight();
    int gridWidth = Renderer::GetGridWidth();
    int gridHeight = Renderer::GetGridHeight();
    int iconX = (screenWidth * Renderer::GetDetailsPanelCol()) / gridWidth;
    int iconY = (screenHeight * (LIST_START_ROW + 1)) / gridHeight;

    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, iconSize, iconSize);
    } else {
        Renderer::DrawPlaceholder(iconX, iconY, iconSize, iconSize, 0x333333FF);
    }

    // Calculate info row based on icon position
    // Icon takes about 4-6 rows depending on screen size
    int iconRows = (iconSize / Renderer::GetScreenConfig().charHeight) + 1;
    int infoStartRow = LIST_START_ROW + 1 + iconRows;

    // Ensure info doesn't overlap footer
    if (infoStartRow + 5 > footerRow) {
        infoStartRow = footerRow - 5;
    }
    if (infoStartRow < LIST_START_ROW + 2) {
        infoStartRow = LIST_START_ROW + 2;
    }

    int currentRow = infoStartRow;

    // Title ID
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "ID: %016llX",
             static_cast<unsigned long long>(title->titleId));
    Renderer::DrawText(detailsCol, currentRow++, idStr);

    // Favorite status
    const char* favStatus = Settings::IsFavorite(title->titleId) ? "Yes" : "No";
    Renderer::DrawTextF(detailsCol, currentRow++, "Favorite: %s", favStatus);

    // Game ID (product code) for debugging preset matching
    if (title->productCode[0] != '\0') {
        Renderer::DrawTextF(detailsCol, currentRow++, "Game ID: %s", title->productCode);
    } else {
        Renderer::DrawText(detailsCol, currentRow++, "Game ID: (none)");
    }

    // Look up preset metadata if available
    const TitlePresets::TitlePreset* preset = nullptr;
    if (title->productCode[0] != '\0') {
        preset = TitlePresets::GetPresetByGameId(title->productCode);
    }

    // Display preset metadata if found
    if (preset) {
        currentRow++;  // Blank line before preset info

        if (preset->publisher[0] != '\0') {
            Renderer::DrawTextF(detailsCol, currentRow++, "Pub: %s", preset->publisher);
        }

        if (preset->developer[0] != '\0' && currentRow < footerRow - 3) {
            Renderer::DrawTextF(detailsCol, currentRow++, "Dev: %s", preset->developer);
        }

        // Release date
        if (preset->releaseYear > 0 && currentRow < footerRow - 3) {
            if (preset->releaseMonth > 0 && preset->releaseDay > 0) {
                Renderer::DrawTextF(detailsCol, currentRow++, "Released: %04d-%02d-%02d",
                                   preset->releaseYear, preset->releaseMonth, preset->releaseDay);
            } else if (preset->releaseMonth > 0) {
                Renderer::DrawTextF(detailsCol, currentRow++, "Released: %04d-%02d",
                                   preset->releaseYear, preset->releaseMonth);
            } else {
                Renderer::DrawTextF(detailsCol, currentRow++, "Released: %04d",
                                   preset->releaseYear);
            }
        }

        // Genre and region combined if space allows
        if (currentRow < footerRow - 3) {
            if (preset->genre[0] != '\0' && preset->region[0] != '\0') {
                Renderer::DrawTextF(detailsCol, currentRow++, "%s / %s",
                                   preset->genre, preset->region);
            } else if (preset->genre[0] != '\0') {
                Renderer::DrawTextF(detailsCol, currentRow++, "Genre: %s", preset->genre);
            } else if (preset->region[0] != '\0') {
                Renderer::DrawTextF(detailsCol, currentRow++, "Region: %s", preset->region);
            }
        }
    }

    // Categories this title belongs to (only if there's room)
    if (currentRow < footerRow - 2) {
        currentRow++;  // Blank line before categories
        Renderer::DrawText(detailsCol, currentRow++, "Categories:");

        uint16_t catIds[Settings::MAX_CATEGORIES];
        int catCount = Settings::GetCategoriesForTitle(title->titleId, catIds, Settings::MAX_CATEGORIES);

        if (catCount == 0) {
            Renderer::DrawText(detailsCol + 2, currentRow, "(none)");
        } else {
            for (int i = 0; i < catCount && currentRow < footerRow - 1; i++) {
                const Settings::Category* cat = Settings::GetCategory(catIds[i]);
                if (cat) {
                    Renderer::DrawTextF(detailsCol + 2, currentRow++, "- %s", cat->name);
                }
            }
        }
    }
}

void drawFooter() {
    int count = Categories::GetFilteredCount();
    int footerRow = Renderer::GetFooterRow();

    char footer[128];
    snprintf(footer, sizeof(footer),
             "%s:Go %s:Close %s:Fav %s:Edit %s:Settings ZL/ZR:Cat [%d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             Buttons::Actions::FAVORITE.label,
             Buttons::Actions::EDIT.label,
             Buttons::Actions::SETTINGS.label,
             sSelectedIndex + 1,
             count);

    Renderer::DrawText(0, footerRow, footer);
}

void drawHeader() {
    // Draw horizontal divider line that spans the screen width
    int width = Renderer::GetGridWidth();
    char line[256];
    int len = std::min(width, (int)sizeof(line) - 1);
    memset(line, '-', len);
    line[len] = '\0';
    Renderer::DrawText(0, HEADER_ROW, line);
}

// =============================================================================
// Main Render Function
// =============================================================================

void renderBrowseMode() {
    drawCategoryBar();
    drawHeader();
    drawTitleList();
    drawDetailsPanel();
    drawDivider();  // Draw divider AFTER content so it's not overwritten
    drawFooter();
}

void renderFrame(uint32_t bgColor) {
    Renderer::BeginFrame(bgColor);
    renderBrowseMode();
    Renderer::EndFrame();
}

} // namespace MenuRender
