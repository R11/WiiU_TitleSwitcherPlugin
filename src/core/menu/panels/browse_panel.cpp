/**
 * Browse Panel Implementation
 * Main browsing mode - title list and details panel.
 */

#include "browse_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../categories.h"
#include "../../render/renderer.h"
#include "../../render/image_loader.h"
#include "../../render/measurements.h"
#include "../../input/buttons.h"
#include "../../titles/titles.h"
#include "../../storage/settings.h"
#include "../../presets/title_presets.h"
#include "../../ui/list_view.h"

#include <cstdio>
#include <cstring>

namespace Menu {
namespace BrowsePanel {

using namespace Internal;

namespace {

void drawCategoryBar()
{
    char line[80];
    int col = 1;

    const Settings::PluginSettings& settings = Settings::Get();

    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    for (int i = 0; i < catCount && col < Measurements::CATEGORY_BAR_MAX_WIDTH; i++) {
        if (!Categories::IsCategoryVisible(i)) {
            continue;
        }

        const char* name = Categories::GetCategoryName(i);
        uint32_t color;

        if (i == currentCat) {
            snprintf(line, sizeof(line), "[%s] ", name);
            color = settings.highlightedTitleColor;
        } else {
            snprintf(line, sizeof(line), " %s  ", name);
            color = settings.categoryColor;
        }

        Renderer::DrawText(col, CATEGORY_ROW, line, color);
        col += strlen(line);
    }
}

void drawDivider()
{
    for (int row = LIST_START_ROW; row < Renderer::GetFooterRow(); row++) {
        Renderer::DrawText(Renderer::GetDividerCol(), row, "|");
    }
}

void drawTitleList()
{
    int count = Categories::GetFilteredCount();
    sTitleListState.itemCount = count;

    UI::ListView::Config listConfig = UI::ListView::LeftPanelConfig(Renderer::GetVisibleRows());
    listConfig.width = Renderer::GetListWidth();
    listConfig.showLineNumbers = Settings::Get().showNumbers;

    UI::ListView::Render(sTitleListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(index);

        if (!title) {
            view.text = "(error)";
            return view;
        }

        view.text = title->name;
        view.prefix = isSelected ? "> " : "  ";

        const Settings::PluginSettings& settings = Settings::Get();
        bool isFavorite = Settings::IsFavorite(title->titleId);

        if (isSelected) {
            view.textColor = settings.highlightedTitleColor;
            view.prefixColor = settings.highlightedTitleColor;
        } else if (isFavorite) {
            view.textColor = settings.favoriteColor;
        } else {
            view.textColor = settings.titleColor;
        }

        if (settings.showFavorites && isFavorite) {
            view.suffix = " *";
        }

        return view;
    });
}

void drawDetailsPanelHeader(const Titles::TitleInfo* title)
{
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, title->name);

    ImageLoader::Request(title->titleId, ImageLoader::Priority::HIGH);

    const Layout::PixelLayout& layout = Renderer::GetLayout();
    int iconX = layout.details.icon.x;
    int iconY = layout.details.icon.y;
    int iconSize = layout.iconSize;

    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, iconSize, iconSize);
    } else {
        Renderer::DrawPlaceholder(iconX, iconY, iconSize, iconSize, 0x333333FF);
    }
}

void drawDetailsPanelBasicInfo(const Titles::TitleInfo* title, int& currentRow)
{
    int col = Renderer::GetDetailsPanelCol();

    char idStr[32];
    snprintf(idStr, sizeof(idStr), "ID: %016llX",
             static_cast<unsigned long long>(title->titleId));
    Renderer::DrawText(col, currentRow++, idStr);

    const char* favStatus = Settings::IsFavorite(title->titleId) ? "Yes" : "No";
    Renderer::DrawTextF(col, currentRow++, "Favorite: %s", favStatus);

    if (title->productCode[0] != '\0') {
        Renderer::DrawTextF(col, currentRow++, "Game ID: %s", title->productCode);
    } else {
        Renderer::DrawText(col, currentRow++, "Game ID: (none)");
    }
}

void drawDetailsPanelPreset(const TitlePresets::TitlePreset* preset, int& currentRow)
{
    if (!preset) return;

    int col = Renderer::GetDetailsPanelCol();
    int maxRow = Renderer::GetFooterRow() - 3;

    currentRow++;

    if (preset->publisher[0] != '\0') {
        Renderer::DrawTextF(col, currentRow++, "Pub: %s", preset->publisher);
    }

    if (preset->developer[0] != '\0' && currentRow < maxRow) {
        Renderer::DrawTextF(col, currentRow++, "Dev: %s", preset->developer);
    }

    if (preset->releaseYear > 0 && currentRow < maxRow) {
        if (preset->releaseMonth > 0 && preset->releaseDay > 0) {
            Renderer::DrawTextF(col, currentRow++, "Released: %04d-%02d-%02d",
                               preset->releaseYear, preset->releaseMonth, preset->releaseDay);
        } else if (preset->releaseMonth > 0) {
            Renderer::DrawTextF(col, currentRow++, "Released: %04d-%02d",
                               preset->releaseYear, preset->releaseMonth);
        } else {
            Renderer::DrawTextF(col, currentRow++, "Released: %04d", preset->releaseYear);
        }
    }

    if (currentRow < maxRow) {
        if (preset->genre[0] != '\0' && preset->region[0] != '\0') {
            Renderer::DrawTextF(col, currentRow++, "%s / %s", preset->genre, preset->region);
        } else if (preset->genre[0] != '\0') {
            Renderer::DrawTextF(col, currentRow++, "Genre: %s", preset->genre);
        } else if (preset->region[0] != '\0') {
            Renderer::DrawTextF(col, currentRow++, "Region: %s", preset->region);
        }
    }
}

void drawDetailsPanelCategories(uint64_t titleId, int& currentRow)
{
    if (currentRow >= Renderer::GetFooterRow() - 2) return;

    int col = Renderer::GetDetailsPanelCol();

    currentRow++;
    Renderer::DrawText(col, currentRow++, "Categories:");

    uint16_t catIds[Settings::MAX_CATEGORIES];
    int catCount = Settings::GetCategoriesForTitle(titleId, catIds, Settings::MAX_CATEGORIES);

    if (catCount == 0) {
        Renderer::DrawText(col + Measurements::INDENT_SUB_ITEM, currentRow, "(none)");
    } else {
        for (int i = 0; i < catCount && currentRow < Renderer::GetFooterRow() - 1; i++) {
            const Settings::Category* cat = Settings::GetCategory(catIds[i]);
            if (cat) {
                Renderer::DrawTextF(col + Measurements::INDENT_SUB_ITEM, currentRow++, "- %s", cat->name);
            }
        }
    }
}

void drawDetailsPanel()
{
    int count = Categories::GetFilteredCount();
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    if (count == 0 || selectedIdx < 0 || selectedIdx >= count) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
    if (!title) return;

    drawDetailsPanelHeader(title);

    int currentRow = Measurements::GetInfoStartRow(LIST_START_ROW);

    drawDetailsPanelBasicInfo(title, currentRow);

    const TitlePresets::TitlePreset* preset = nullptr;
    if (title->productCode[0] != '\0') {
        preset = TitlePresets::GetPresetByGameId(title->productCode);
    }
    drawDetailsPanelPreset(preset, currentRow);

    drawDetailsPanelCategories(title->titleId, currentRow);
}

void drawFooter()
{
    int count = Categories::GetFilteredCount();
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);

    int pending, ready, failed, total;
    ImageLoader::GetLoadingStats(&pending, &ready, &failed, &total);

    char footer[120];
    snprintf(footer, sizeof(footer),
             "%s:Go %s:Close %s:Fav %s:Edit %s:Settings ZL/ZR:Cat [%d/%d] %d/%d",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             Buttons::Actions::FAVORITE.label,
             Buttons::Actions::EDIT.label,
             Buttons::Actions::SETTINGS.label,
             selectedIdx + 1,
             count,
             ready,
             total);

    Renderer::DrawText(1, Renderer::GetFooterRow(), footer);
}

}

void Render()
{
    drawCategoryBar();
    drawHeaderDivider();
    drawDivider();
    drawTitleList();
    drawDetailsPanel();
    drawFooter();
}

uint64_t HandleInput(uint32_t pressed)
{
    int count = Categories::GetFilteredCount();
    sTitleListState.itemCount = count;

    UI::ListView::Config listConfig = UI::ListView::BrowseModeConfig(Renderer::GetVisibleRows());

    UI::ListView::HandleInput(sTitleListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);

    if (Buttons::Actions::CATEGORY_PREV.Pressed(pressed)) {
        Categories::PreviousCategory();
        sTitleListState = UI::ListView::State();
        clampSelection();
    }
    if (Buttons::Actions::CATEGORY_NEXT.Pressed(pressed)) {
        Categories::NextCategory();
        sTitleListState = UI::ListView::State();
        clampSelection();
    }

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::FAVORITE:
            if (isValidSelection(selectedIdx, count)) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
                if (title) {
                    Settings::ToggleFavorite(title->titleId);
                    Categories::RefreshFilter();
                    clampSelection();
                }
            }
            break;

        case UI::ListView::Action::CANCEL:
            sIsOpen = false;
            return 0;

        case UI::ListView::Action::CONFIRM:
            if (isValidSelection(selectedIdx, count)) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
                if (title) {
                    sIsOpen = false;
                    return title->titleId;
                }
            }
            break;

        default:
            break;
    }

    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        sEditCatsListState = UI::ListView::State();
        sCurrentMode = Mode::EDIT;
    }

    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        sSettingsListState = UI::ListView::State();
        sSettingsSubMode = SettingsSubMode::MAIN;
        sCurrentMode = Mode::SETTINGS;
    }

    return 0;
}

}
}
