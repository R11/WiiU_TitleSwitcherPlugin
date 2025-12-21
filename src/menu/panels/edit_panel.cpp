/**
 * Edit Panel Implementation
 * Category assignment mode for titles.
 */

#include "edit_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../categories.h"
#include "../../render/renderer.h"
#include "../../render/measurements.h"
#include "../../input/buttons.h"
#include "../../titles/titles.h"
#include "../../storage/settings.h"
#include "../../ui/list_view.h"

#include <cstdio>
#include <cstring>

namespace Menu {
namespace EditPanel {

using namespace Internal;

namespace {

void drawDivider()
{
    for (int row = LIST_START_ROW; row < Renderer::GetFooterRow(); row++) {
        Renderer::DrawText(Renderer::GetDividerCol(), row, "|");
    }
}

}

void Render()
{
    int titleIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(titleIdx);
    if (!title) {
        Renderer::DrawText(0, 0, "Error: No title selected");
        return;
    }

    Renderer::DrawText(0, 0, "EDIT TITLE CATEGORIES");
    drawHeaderDivider();

    char nameLine[32];
    strncpy(nameLine, title->name, 28);
    nameLine[28] = '\0';
    Renderer::DrawTextF(0, LIST_START_ROW, "> %s", nameLine);

    Renderer::DrawTextF(0, LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_START, "ID: %016llX",
                      static_cast<unsigned long long>(title->titleId));

    drawDivider();

    drawDetailsPanelSectionHeader("Categories:");

    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;
    uint64_t titleId = title->titleId;

    if (catCount == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START,
                         "No categories defined.");
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_LINE2,
                         "Create in Settings (+)");
    } else {
        UI::ListView::Config listConfig = UI::ListView::DetailsPanelConfig(
            Measurements::ROW_OFFSET_SECTION_START,
            Measurements::CATEGORY_EDIT_VISIBLE_ROWS);
        listConfig.canToggle = true;

        sEditCatsListState.itemCount = catCount;

        UI::ListView::Render(sEditCatsListState, listConfig, [&categories, titleId](int index, bool isSelected) {
            UI::ListView::ItemView view;
            const Settings::Category& cat = categories[index];
            bool inCategory = Settings::TitleHasCategory(titleId, cat.id);

            static char prefixBuf[16];
            snprintf(prefixBuf, sizeof(prefixBuf), "%s%s ",
                    isSelected ? ">" : " ",
                    inCategory ? "[X]" : "[ ]");
            view.prefix = prefixBuf;
            view.text = cat.name;
            return view;
        });
    }

    int selectedIdx = UI::ListView::GetSelectedIndex(sEditCatsListState);
    char footer[80];
    snprintf(footer, sizeof(footer),
             "%s:Toggle %s:Back  [Category %d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             catCount > 0 ? selectedIdx + 1 : 0,
             catCount > 0 ? catCount : 1);
    Renderer::DrawText(0, Renderer::GetFooterRow(), footer);
}

void HandleInput(uint32_t pressed)
{
    int titleIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(titleIdx);
    if (!title) {
        sCurrentMode = Mode::BROWSE;
        return;
    }

    int catCount = Settings::GetCategoryCount();

    UI::ListView::Config listConfig = UI::ListView::EditModeConfig(Measurements::CATEGORY_EDIT_VISIBLE_ROWS);

    sEditCatsListState.itemCount = catCount;

    UI::ListView::HandleInput(sEditCatsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sEditCatsListState);

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::TOGGLE:
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount) {
                const auto& categories = Settings::Get().categories;
                uint16_t catId = categories[selectedIdx].id;

                if (Settings::TitleHasCategory(title->titleId, catId)) {
                    Settings::RemoveTitleFromCategory(title->titleId, catId);
                } else {
                    Settings::AssignTitleToCategory(title->titleId, catId);
                }
                Categories::RefreshFilter();
            }
            break;

        case UI::ListView::Action::CANCEL:
            Settings::Save();
            sCurrentMode = Mode::BROWSE;
            break;

        default:
            break;
    }
}

}
}
