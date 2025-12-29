/**
 * Grid Panel Implementation
 *
 * Icon grid browsing mode with dual-screen support.
 */

#include "grid_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../categories.h"
#include "../../render/renderer.h"
#include "../../render/image_loader.h"
#include "../../input/buttons.h"
#include "../../titles/titles.h"
#include "../../storage/settings.h"
#include "../../presets/title_presets.h"
#include "../../ui/grid_view.h"
#include "../../ui/details_view.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace Menu {
namespace GridPanel {

using namespace Internal;

namespace {

GridView::State sGridState;
GridView::IconSize sIconSize = GridView::IconSize::MEDIUM;

void drawCategoryBar(const Screen::Descriptor& screen, const GridView::Layout& layout)
{
    const Settings::PluginSettings& settings = Settings::Get();

    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    int col = screen.contentX() + screen.scaled(8);
    int y = layout.categoryBarY + screen.scaled(8);

    char line[80];

    for (int i = 0; i < catCount; i++) {
        if (!Categories::IsCategoryVisible(i)) {
            continue;
        }

        const char* name = Categories::GetCategoryName(i);
        uint32_t color;

        if (i == currentCat) {
            snprintf(line, sizeof(line), "[%s]", name);
            color = settings.highlightedTitleColor;
        } else {
            snprintf(line, sizeof(line), " %s ", name);
            color = settings.categoryColor;
        }

        Renderer::DrawText(col / 8, y / 24, line, color);
        col += (strlen(line) + 1) * screen.scaled(8);
    }
}

void drawGridCell(const Screen::Descriptor& screen,
                  const GridView::Layout& layout,
                  int cellX, int cellY,
                  int titleIndex, bool isSelected)
{
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(titleIndex);
    if (!title) return;

    const Settings::PluginSettings& settings = Settings::Get();

    int iconX = cellX + (layout.cellWidth - layout.iconSize) / 2;
    int iconY = cellY + (layout.cellHeight - layout.iconSize) / 2;

    // Selection highlight border
    if (isSelected) {
        int borderWidth = layout.selectionBorderWidth;
        uint32_t highlightColor = settings.highlightedTitleColor;

        // Draw border rectangle
        int bx = iconX - borderWidth;
        int by = iconY - borderWidth;
        int bw = layout.iconSize + (borderWidth * 2);
        int bh = layout.iconSize + (borderWidth * 2);

        // Top edge
        for (int i = 0; i < borderWidth; i++) {
            Renderer::DrawHLine(bx, by + i, bw, highlightColor);
        }
        // Bottom edge
        for (int i = 0; i < borderWidth; i++) {
            Renderer::DrawHLine(bx, by + bh - borderWidth + i, bw, highlightColor);
        }
        // Left edge
        for (int i = 0; i < borderWidth; i++) {
            Renderer::DrawVLine(bx + i, by, bh, highlightColor);
        }
        // Right edge
        for (int i = 0; i < borderWidth; i++) {
            Renderer::DrawVLine(bx + bw - borderWidth + i, by, bh, highlightColor);
        }
    }

    // Request icon with appropriate priority
    ImageLoader::Priority priority = isSelected ?
        ImageLoader::Priority::HIGH : ImageLoader::Priority::NORMAL;
    ImageLoader::Request(title->titleId, priority);

    // Draw icon or placeholder
    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, layout.iconSize, layout.iconSize);
    } else {
        // Placeholder with game name abbreviation
        Renderer::DrawPlaceholder(iconX, iconY, layout.iconSize, layout.iconSize, 0x313244FF);

        // Abbreviate and center the title name
        char abbrev[16];
        int maxChars = layout.iconSize / screen.scaled(8);
        if (maxChars > 14) maxChars = 14;
        if (maxChars < 4) maxChars = 4;

        strncpy(abbrev, title->name, maxChars);
        abbrev[maxChars] = '\0';

        int textWidth = strlen(abbrev) * screen.scaled(8);
        int textX = iconX + (layout.iconSize - textWidth) / 2;
        int textY = iconY + (layout.iconSize / 2) - screen.scaled(8);

        // Convert to grid coordinates for DrawText
        Renderer::DrawText(textX / 8, textY / 24, abbrev, 0x6C7086FF);
    }
}

void drawIconGrid(const Screen::Descriptor& screen, const GridView::Layout& layout)
{
    int count = sGridState.totalItems;
    if (count == 0) return;

    int firstVisibleIndex = sGridState.scrollRow * layout.columns;
    int cellsOnScreen = layout.getTotalVisibleCells();

    for (int i = 0; i < cellsOnScreen; i++) {
        int titleIndex = firstVisibleIndex + i;
        if (titleIndex >= count) break;

        int col = i % layout.columns;
        int row = i / layout.columns;

        int cellX = layout.getCellX(col);
        int cellY = layout.getCellY(row);
        bool isSelected = (titleIndex == sGridState.selectedIndex);

        drawGridCell(screen, layout, cellX, cellY, titleIndex, isSelected);
    }
}

void drawTitleBar(const Screen::Descriptor& screen, const GridView::Layout& layout)
{
    if (sGridState.selectedIndex < 0 || sGridState.selectedIndex >= sGridState.totalItems) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sGridState.selectedIndex);
    if (!title) return;

    const Settings::PluginSettings& settings = Settings::Get();

    // Draw selected title name centered
    char displayText[128];
    snprintf(displayText, sizeof(displayText), "> %s", title->name);

    int textWidth = strlen(displayText) * screen.scaled(8);
    int textX = (screen.width - textWidth) / 2;

    Renderer::DrawText(textX / 8, layout.titleBarY / 24, displayText, settings.highlightedTitleColor);

    // Draw button hints on the right
    const char* hints = "[A] Go  [X] Size";
    int hintsWidth = strlen(hints) * screen.scaled(8);
    int hintsX = screen.width - screen.marginX - hintsWidth;

    Renderer::DrawText(hintsX / 8, layout.titleBarY / 24, hints, 0x6C7086FF);
}

void drawScrollIndicator(const Screen::Descriptor& screen, const GridView::Layout& layout)
{
    int totalRows = sGridState.totalRows(layout.columns);
    if (totalRows <= layout.visibleRows) return;

    // Draw scroll position indicator on right edge
    int indicatorHeight = screen.scaled(4);
    int trackHeight = layout.gridHeight - screen.scaled(20);
    int trackY = layout.gridStartY + screen.scaled(10);

    float scrollRatio = static_cast<float>(sGridState.scrollRow) /
                        static_cast<float>(totalRows - layout.visibleRows);
    int indicatorY = trackY + static_cast<int>(scrollRatio * (trackHeight - indicatorHeight));

    int indicatorX = layout.gridStartX + layout.gridWidth + screen.scaled(4);

    // Draw track
    Renderer::DrawVLine(indicatorX + 1, trackY, trackHeight, 0x45475AFF);

    // Draw indicator
    for (int i = 0; i < indicatorHeight; i++) {
        Renderer::DrawHLine(indicatorX, indicatorY + i, screen.scaled(4), 0x89B4FAFF);
    }
}

void renderGrid(const Screen::Descriptor& screen)
{
    GridView::Layout layout = GridView::ComputeLayout(screen, sIconSize);

    drawCategoryBar(screen, layout);
    drawIconGrid(screen, layout);
    drawTitleBar(screen, layout);
    drawScrollIndicator(screen, layout);
}

void drawDetailsArtwork(const Screen::Descriptor& screen,
                        const DetailsView::Layout& layout,
                        uint64_t titleId)
{
    ImageLoader::Request(titleId, ImageLoader::Priority::HIGH);

    if (ImageLoader::IsReady(titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(titleId);
        Renderer::DrawImage(layout.artwork.x, layout.artwork.y,
                           icon, layout.artwork.size, layout.artwork.size);
    } else {
        Renderer::DrawPlaceholder(layout.artwork.x, layout.artwork.y,
                                  layout.artwork.size, layout.artwork.size,
                                  0x313244FF);

        const char* loadingText = "Loading...";
        int textWidth = strlen(loadingText) * screen.scaled(8);
        int textX = layout.artwork.x + (layout.artwork.size - textWidth) / 2;
        int textY = layout.artwork.y + (layout.artwork.size / 2) - screen.scaled(8);

        Renderer::DrawText(textX / 8, textY / 24, loadingText, 0x6C7086FF);
    }
}

void drawInfoRow(const Screen::Descriptor& screen,
                 const DetailsView::Layout& layout,
                 int rowIndex,
                 const char* label,
                 const char* value)
{
    if (!value || value[0] == '\0') return;
    if (rowIndex >= layout.info.maxRows) return;

    int y = layout.info.y + (rowIndex * layout.info.lineHeight);

    // Label in dim color
    Renderer::DrawText(layout.info.x / 8, y / 24, label, 0x6C7086FF);

    // Value in bright color
    Renderer::DrawText(layout.info.valueX / 8, y / 24, value, 0xCDD6F4FF);
}

void renderDetails(const Screen::Descriptor& screen)
{
    if (sGridState.selectedIndex < 0 || sGridState.totalItems == 0) {
        // No selection - show placeholder message
        const char* msg = "No game selected";
        int textX = (screen.width - strlen(msg) * screen.scaled(8)) / 2;
        int textY = screen.height / 2;
        Renderer::DrawText(textX / 8, textY / 24, msg, 0x6C7086FF);
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sGridState.selectedIndex);
    if (!title) return;

    DetailsView::Layout layout = DetailsView::ComputeLayout(screen);
    const Settings::PluginSettings& settings = Settings::Get();

    // Draw large artwork
    drawDetailsArtwork(screen, layout, title->titleId);

    // Draw game title
    Renderer::DrawText(layout.title.x / 8, layout.title.y / 24,
                       title->name, settings.highlightedTitleColor);

    // Draw divider line
    Renderer::DrawHLine(layout.divider.x, layout.divider.y,
                        layout.divider.width, 0x45475AFF);

    // Draw metadata rows
    int row = 0;

    // Try to get GameTDB preset data
    const TitlePresets::TitlePreset* preset = nullptr;
    if (title->productCode[0] != '\0') {
        preset = TitlePresets::GetPresetByGameId(title->productCode);
    }

    if (preset) {
        if (preset->publisher[0] != '\0') {
            drawInfoRow(screen, layout, row++,
                       DetailsView::INFO_LABEL_PUBLISHER, preset->publisher);
        }

        if (preset->developer[0] != '\0') {
            drawInfoRow(screen, layout, row++,
                       DetailsView::INFO_LABEL_DEVELOPER, preset->developer);
        }

        if (preset->releaseYear > 0) {
            char dateStr[32];
            if (preset->releaseMonth > 0 && preset->releaseDay > 0) {
                snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
                        preset->releaseYear, preset->releaseMonth, preset->releaseDay);
            } else if (preset->releaseMonth > 0) {
                snprintf(dateStr, sizeof(dateStr), "%04d-%02d",
                        preset->releaseYear, preset->releaseMonth);
            } else {
                snprintf(dateStr, sizeof(dateStr), "%04d", preset->releaseYear);
            }
            drawInfoRow(screen, layout, row++, DetailsView::INFO_LABEL_RELEASED, dateStr);
        }

        if (preset->genre[0] != '\0') {
            drawInfoRow(screen, layout, row++, DetailsView::INFO_LABEL_GENRE, preset->genre);
        }

        if (preset->region[0] != '\0') {
            drawInfoRow(screen, layout, row++, DetailsView::INFO_LABEL_REGION, preset->region);
        }
    } else {
        // No preset data - show title ID and product code
        char titleIdStr[32];
        snprintf(titleIdStr, sizeof(titleIdStr), "%016llX",
                static_cast<unsigned long long>(title->titleId));
        drawInfoRow(screen, layout, row++, DetailsView::INFO_LABEL_TITLE_ID, titleIdStr);

        if (title->productCode[0] != '\0') {
            drawInfoRow(screen, layout, row++, DetailsView::INFO_LABEL_PRODUCT, title->productCode);
        }
    }

    // Button hints at bottom
    const char* hints = "[A] Launch    [B] Close Menu";
    int hintsWidth = strlen(hints) * screen.scaled(8);
    int hintsX = (screen.width - hintsWidth) / 2;

    Renderer::DrawText(hintsX / 8, layout.hints.y / 24, hints, 0x6C7086FF);
}

} // anonymous namespace

void Init()
{
    sGridState = GridView::State();
    sGridState.totalItems = Categories::GetFilteredCount();

    // Restore selection from list view if available
    int listSelection = UI::ListView::GetSelectedIndex(sTitleListState);
    if (listSelection >= 0 && listSelection < sGridState.totalItems) {
        sGridState.selectedIndex = listSelection;
    }

    // Compute layout for DRC to ensure selection is visible
    Screen::Descriptor drc = Screen::GetDRCDescriptor();
    GridView::Layout layout = GridView::ComputeLayout(drc, sIconSize);
    GridView::EnsureSelectionVisible(sGridState, layout);
}

void Render(const Screen::Descriptor& screen, Screen::ContentMode contentMode)
{
    switch (contentMode) {
        case Screen::ContentMode::GRID:
            renderGrid(screen);
            break;

        case Screen::ContentMode::DETAILS:
            renderDetails(screen);
            break;

        case Screen::ContentMode::OFF:
            break;
    }
}

uint64_t HandleInput(uint32_t pressed)
{
    int count = Categories::GetFilteredCount();
    sGridState.totalItems = count;

    if (count == 0) {
        // Handle empty state
        if (Buttons::Actions::CANCEL.Pressed(pressed)) {
            sIsOpen = false;
        }
        return 0;
    }

    // Compute layout for navigation
    Screen::Descriptor drc = Screen::GetDRCDescriptor();
    GridView::Layout layout = GridView::ComputeLayout(drc, sIconSize);

    // Handle grid navigation
    GridView::HandleNavigation(sGridState, layout, pressed);

    // Category switching
    if (Buttons::Actions::CATEGORY_PREV.Pressed(pressed)) {
        Categories::PreviousCategory();
        sGridState.totalItems = Categories::GetFilteredCount();
        GridView::ClampState(sGridState, layout);
    }
    if (Buttons::Actions::CATEGORY_NEXT.Pressed(pressed)) {
        Categories::NextCategory();
        sGridState.totalItems = Categories::GetFilteredCount();
        GridView::ClampState(sGridState, layout);
    }

    // Icon size cycling
    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        CycleIconSize();
        layout = GridView::ComputeLayout(drc, sIconSize);
        GridView::ClampState(sGridState, layout);
    }

    // Confirm - launch game
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (sGridState.selectedIndex >= 0 && sGridState.selectedIndex < count) {
            const Titles::TitleInfo* title = Categories::GetFilteredTitle(sGridState.selectedIndex);
            if (title) {
                sIsOpen = false;
                return title->titleId;
            }
        }
    }

    // Cancel - close menu
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        sIsOpen = false;
        return 0;
    }

    // Settings
    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        sSettingsListState = UI::ListView::State();
        sSettingsSubMode = SettingsSubMode::MAIN;
        sCurrentMode = Mode::SETTINGS;
    }

    return 0;
}

const GridView::State& GetState()
{
    return sGridState;
}

void SetSelection(int index)
{
    Screen::Descriptor drc = Screen::GetDRCDescriptor();
    GridView::Layout layout = GridView::ComputeLayout(drc, sIconSize);
    GridView::SetSelection(sGridState, layout, index);
}

void CycleIconSize()
{
    int current = static_cast<int>(sIconSize);
    int next = (current + 1) % static_cast<int>(GridView::IconSize::COUNT);
    sIconSize = static_cast<GridView::IconSize>(next);
}

GridView::IconSize GetIconSize()
{
    return sIconSize;
}

void SetIconSize(GridView::IconSize size)
{
    sIconSize = size;
}

}
}
