/**
 * Settings Panel Implementation
 * Settings mode with submodes for colors, categories, system apps.
 */

#include "settings_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../categories.h"
#include "../../render/renderer.h"
#include "../../render/measurements.h"
#include "../../input/buttons.h"
#include "../../storage/settings.h"
#include "../../ui/list_view.h"

#include <sysapp/launch.h>
#include <sysapp/switch.h>
#include <sysapp/title.h>
#include <nn/ccr/sys.h>

#include <cstdio>
#include <cstring>

namespace Menu {
namespace SettingsPanel {

using namespace Internal;

namespace {

void drawDivider()
{
    for (int row = LIST_START_ROW; row < Renderer::GetFooterRow(); row++) {
        Renderer::DrawText(Renderer::GetDividerCol(), row, "|");
    }
}

int getCurrentBrightness()
{
    CCRSysLCDMode mode;
    if (CCRSysGetCurrentLCDMode(&mode) == 0) {
        return static_cast<int>(mode) + 1;
    }
    return 3;
}

void setBrightness(int level)
{
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    CCRSysLCDMode mode = static_cast<CCRSysLCDMode>(level - 1);
    CCRSysSetCurrentLCDMode(mode);
}

void renderSettingsMain()
{
    Renderer::DrawText(0, 0, "SETTINGS");
    drawHeaderDivider();
    drawDivider();

    UI::ListView::Config listConfig = UI::ListView::LeftPanelConfig();
    sSettingsListState.itemCount = SETTINGS_ITEM_COUNT;

    UI::ListView::Render(sSettingsListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        const SettingItem& item = sSettingItems[index];
        view.prefix = isSelected ? "> " : "  ";

        static char textBuf[64];
        switch (item.type) {
            case SettingType::TOGGLE: {
                bool value = *getTogglePtr(item.dataOffset);
                snprintf(textBuf, sizeof(textBuf), "%s: %s", item.name, value ? "ON" : "OFF");
                break;
            }
            case SettingType::COLOR: {
                uint32_t value = *getColorPtr(item.dataOffset);
                snprintf(textBuf, sizeof(textBuf), "%s: %08X", item.name, value);
                break;
            }
            case SettingType::BRIGHTNESS: {
                int brightness = getCurrentBrightness();
                char bar[8] = "-----";
                for (int b = 0; b < brightness && b < 5; b++) bar[b] = '=';
                snprintf(textBuf, sizeof(textBuf), "%s: [%s] %d", item.name, bar, brightness);
                break;
            }
            case SettingType::ACTION: {
                if (item.dataOffset == ACTION_MANAGE_CATEGORIES) {
                    snprintf(textBuf, sizeof(textBuf), "%s (%d)", item.name, Settings::GetCategoryCount());
                } else {
                    snprintf(textBuf, sizeof(textBuf), "%s", item.name);
                }
                break;
            }
        }
        view.text = textBuf;
        return view;
    });

    drawDetailsPanelSectionHeader("Description:");

    int selectedIdx = UI::ListView::GetSelectedIndex(sSettingsListState);
    if (isValidSelection(selectedIdx, SETTINGS_ITEM_COUNT)) {
        const SettingItem& selected = sSettingItems[selectedIdx];
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START, selected.descLine1);
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_LINE2, selected.descLine2);
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_HINT,
                          getSettingActionHint(selected.type));
    }

    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Edit %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      selectedIdx + 1, SETTINGS_ITEM_COUNT);
}

void renderManageCategories()
{
    Renderer::DrawText(0, 0, "MANAGE CATEGORIES");
    drawHeaderDivider();
    drawDivider();

    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    UI::ListView::Config listConfig = UI::ListView::LeftPanelConfig(Measurements::CATEGORY_MANAGE_VISIBLE_ROWS);
    listConfig.canReorder = true;
    listConfig.canDelete = true;

    sManageCatsListState.itemCount = catCount;

    if (catCount == 0) {
        Renderer::DrawText(2, LIST_START_ROW, "(No categories)");
        Renderer::DrawTextF(2, LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_START, "Press %s to create one", Buttons::Actions::SETTINGS.label);
    } else {
        UI::ListView::Render(sManageCatsListState, listConfig, [&categories](int index, bool isSelected) {
            UI::ListView::ItemView view;
            const Settings::Category& cat = categories[index];
            view.text = cat.name;
            view.prefix = isSelected ? "> " : "  ";
            view.suffix = cat.hidden ? " (hidden)" : "";
            return view;
        });
    }

    drawDetailsPanelSectionHeader("Actions:", true);

    int selectedIdx = UI::ListView::GetSelectedIndex(sManageCatsListState);
    if (catCount == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START, "No categories yet.");
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_GAP, "%s: Add New",
                          Buttons::Actions::SETTINGS.label);
    } else if (selectedIdx >= 0 && selectedIdx < catCount) {
        const Settings::Category& cat = categories[selectedIdx];
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START, "Category: %s", cat.name);

        const char* visStatus = cat.hidden ? "Hidden" : "Visible";
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_LINE2, "Status: %s", visStatus);

        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_HINT, "%s: Rename",
                          Buttons::Actions::CONFIRM.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_START, "%s: Delete",
                          Buttons::Actions::EDIT.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE2, "%s: %s",
                          Buttons::Actions::FAVORITE.label,
                          cat.hidden ? "Show" : "Hide");
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE3, "%s: Add New",
                          Buttons::Actions::SETTINGS.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_ACTIONS, "%s/%s: Move Up/Down",
                          Buttons::Actions::NAV_PAGE_UP.label,
                          Buttons::Actions::NAV_PAGE_DOWN.label);
    }

    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Rename %s:Back %s:Hide %s:Add  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      Buttons::Actions::FAVORITE.label,
                      Buttons::Actions::SETTINGS.label,
                      catCount > 0 ? selectedIdx + 1 : 0, catCount);
}

void renderColorInput()
{
    Renderer::DrawText(0, 0, "EDIT COLOR");
    drawHeaderDivider();

    const char* colorName = "Unknown";
    if (sColorReturnSubmode == SettingsSubMode::COLORS && sEditingSettingIndex >= 0) {
        colorName = sColorOptions[sEditingSettingIndex].name;
    }
    Renderer::DrawTextF(0, LIST_START_ROW, "Editing: %s", colorName);
    Renderer::DrawText(0, LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_START, "Enter RGBA hex value (8 digits):");

    sInputField.Render(0, LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_LINE2);

    Renderer::DrawText(0, LIST_START_ROW + Measurements::ROW_OFFSET_INFO_START, "Up/Down: Change character");
    Renderer::DrawTextF(0, LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE2, "%s/%s: Move cursor",
                      Buttons::Actions::INPUT_RIGHT.label,
                      Buttons::Actions::INPUT_LEFT.label);
    Renderer::DrawTextF(0, LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE3, "%s: Delete  %s: Confirm  %s: Cancel",
                      Buttons::Actions::INPUT_DELETE.label,
                      Buttons::Actions::INPUT_CONFIRM.label,
                      Buttons::Actions::INPUT_CANCEL.label);
}

void renderNameInput()
{
    Renderer::DrawText(0, 0, "CATEGORY NAME");
    drawHeaderDivider();

    if (sEditingCategoryId < 0) {
        Renderer::DrawText(0, LIST_START_ROW, "Enter name for new category:");
    } else {
        Renderer::DrawText(0, LIST_START_ROW, "Enter new name:");
    }

    sInputField.Render(0, LIST_START_ROW + Measurements::ROW_OFFSET_CONTENT_START);

    Renderer::DrawText(0, LIST_START_ROW + Measurements::ROW_OFFSET_GAP, "Up/Down: Change character");
    Renderer::DrawTextF(0, LIST_START_ROW + Measurements::ROW_OFFSET_HINT, "%s/%s: Move cursor",
                      Buttons::Actions::INPUT_RIGHT.label,
                      Buttons::Actions::INPUT_LEFT.label);
    Renderer::DrawTextF(0, LIST_START_ROW + Measurements::ROW_OFFSET_INFO_START, "%s: Delete  %s: Confirm  %s: Cancel",
                      Buttons::Actions::INPUT_DELETE.label,
                      Buttons::Actions::INPUT_CONFIRM.label,
                      Buttons::Actions::INPUT_CANCEL.label);
}

void renderColors()
{
    Renderer::DrawText(0, 0, "CUSTOMIZE COLORS");
    drawHeaderDivider();
    drawDivider();

    UI::ListView::Config listConfig = UI::ListView::LeftPanelConfig();
    sColorsListState.itemCount = COLOR_OPTION_COUNT;

    UI::ListView::Render(sColorsListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        const ColorOption& opt = sColorOptions[index];
        view.prefix = isSelected ? "> " : "  ";

        static char textBuf[64];
        uint32_t value = *getColorPtr(opt.dataOffset);
        snprintf(textBuf, sizeof(textBuf), "%s: %08X", opt.name, value);
        view.text = textBuf;
        return view;
    });

    drawDetailsPanelSectionHeader("Preview:");

    int selectedIdx = UI::ListView::GetSelectedIndex(sColorsListState);
    if (isValidSelection(selectedIdx, COLOR_OPTION_COUNT)) {
        const ColorOption& opt = sColorOptions[selectedIdx];
        uint32_t color = *getColorPtr(opt.dataOffset);

        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START,
                          "Sample Text", color);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_GAP,
                          "RGBA: %08X", color);
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_HINT,
                          "A: Edit color");
    }

    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Edit %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      selectedIdx + 1, COLOR_OPTION_COUNT);
}

void renderSystemApps()
{
    Renderer::DrawText(0, 0, "SYSTEM APPS");
    drawHeaderDivider();
    drawDivider();

    UI::ListView::Config listConfig = UI::ListView::LeftPanelConfig();
    sSystemAppsListState.itemCount = SYSTEM_APP_COUNT;

    UI::ListView::Render(sSystemAppsListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        view.text = sSystemApps[index].name;
        view.prefix = isSelected ? "> " : "  ";
        return view;
    });

    drawDetailsPanelSectionHeader("Description:");

    int selectedIdx = UI::ListView::GetSelectedIndex(sSystemAppsListState);
    if (isValidSelection(selectedIdx, SYSTEM_APP_COUNT)) {
        const SystemAppOption& app = sSystemApps[selectedIdx];
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_SECTION_START, app.description);
    }

    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_GAP, "Press A to launch");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_START, "Note: The game will be");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE2, "suspended while the");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + Measurements::ROW_OFFSET_INFO_LINE3, "system app is open.");

    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Launch %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      selectedIdx + 1, SYSTEM_APP_COUNT);
}

void handleSettingsMainInput(uint32_t pressed)
{
    UI::ListView::Config listConfig = UI::ListView::InputOnlyConfig(
        Renderer::GetFooterRow() - LIST_START_ROW - 1);
    listConfig.canConfirm = true;
    listConfig.canCancel = true;
    listConfig.smallSkip = 1;

    sSettingsListState.itemCount = SETTINGS_ITEM_COUNT;

    UI::ListView::HandleInput(sSettingsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sSettingsListState);

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    if (action == UI::ListView::Action::CONFIRM && selectedIdx >= 0) {
        const SettingItem& item = sSettingItems[selectedIdx];

        switch (item.type) {
            case SettingType::TOGGLE: {
                bool* value = getTogglePtr(item.dataOffset);
                *value = !(*value);
                break;
            }
            case SettingType::BRIGHTNESS: {
                int brightness = getCurrentBrightness();
                setBrightness((brightness % 5) + 1);
                break;
            }
            case SettingType::COLOR:
                break;
            case SettingType::ACTION: {
                if (item.dataOffset == ACTION_MANAGE_CATEGORIES) {
                    sManageCatsListState = UI::ListView::State();
                    sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
                } else if (item.dataOffset == ACTION_SYSTEM_APPS) {
                    sSystemAppsListState = UI::ListView::State();
                    sSettingsSubMode = SettingsSubMode::SYSTEM_APPS;
                } else if (item.dataOffset == ACTION_COLORS) {
                    sColorsListState = UI::ListView::State();
                    sSettingsSubMode = SettingsSubMode::COLORS;
                } else if (item.dataOffset == ACTION_DEBUG_GRID) {
                    sCurrentMode = Mode::DEBUG_GRID;
                } else if (item.dataOffset == ACTION_MIIVERSE_TEST) {
                    sCurrentMode = Mode::MIIVERSE_TEST;
                }
                break;
            }
        }
    }

    if (action == UI::ListView::Action::CANCEL) {
        Settings::Save();
        sCurrentMode = Mode::BROWSE;
    }
}

void handleManageCategoriesInput(uint32_t pressed)
{
    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    UI::ListView::Config listConfig = UI::ListView::InputOnlyConfig(Measurements::CATEGORY_MANAGE_VISIBLE_ROWS);
    listConfig.canConfirm = true;
    listConfig.canCancel = true;
    listConfig.canReorder = true;
    listConfig.canDelete = true;
    listConfig.canFavorite = true;

    sManageCatsListState.itemCount = catCount;

    UI::ListView::HandleInput(sManageCatsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sManageCatsListState);

    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        sEditingCategoryId = -1;
        sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
        sInputField.Clear();
        sSettingsSubMode = SettingsSubMode::NAME_INPUT;
        return;
    }

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::CONFIRM:
            if (isValidSelection(selectedIdx, catCount)) {
                const Settings::Category& cat = categories[selectedIdx];
                sEditingCategoryId = cat.id;
                sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
                sInputField.SetValue(cat.name);
                sSettingsSubMode = SettingsSubMode::NAME_INPUT;
            }
            break;

        case UI::ListView::Action::DELETE:
            if (isValidSelection(selectedIdx, catCount)) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::DeleteCategory(catId);
                sManageCatsListState.SetItemCount(Settings::GetCategoryCount(), listConfig.visibleRows);
                Categories::RefreshFilter();
            }
            break;

        case UI::ListView::Action::FAVORITE:
            if (isValidSelection(selectedIdx, catCount)) {
                uint16_t catId = categories[selectedIdx].id;
                bool currentlyHidden = Settings::IsCategoryHidden(catId);
                Settings::SetCategoryHidden(catId, !currentlyHidden);
            }
            break;

        case UI::ListView::Action::MOVE_UP:
            if (catCount > 0 && selectedIdx > 0 && selectedIdx < catCount) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::MoveCategoryUp(catId);
                sManageCatsListState.selectedIndex--;
            }
            break;

        case UI::ListView::Action::MOVE_DOWN:
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount - 1) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::MoveCategoryDown(catId);
                sManageCatsListState.selectedIndex++;
            }
            break;

        case UI::ListView::Action::CANCEL:
            sSettingsSubMode = SettingsSubMode::MAIN;
            break;

        default:
            break;
    }
}

void handleColorInputInput(uint32_t pressed, uint32_t held)
{
    TextInput::Result result = sInputField.HandleInput(pressed, held);

    if (result == TextInput::Result::CONFIRMED) {
        char hexStr[16];
        sInputField.GetValue(hexStr, sizeof(hexStr));

        uint32_t value = 0;
        for (int i = 0; hexStr[i] != '\0' && i < 8; i++) {
            char c = hexStr[i];
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= (c - '0');
            } else if (c >= 'A' && c <= 'F') {
                value |= (c - 'A' + 10);
            } else if (c >= 'a' && c <= 'f') {
                value |= (c - 'a' + 10);
            }
        }

        if (sEditingColorOffset >= 0) {
            uint32_t* color = getColorPtr(sEditingColorOffset);
            *color = value;
        }

        sEditingSettingIndex = -1;
        sEditingColorOffset = -1;
        sSettingsSubMode = sColorReturnSubmode;
    } else if (result == TextInput::Result::CANCELLED) {
        sEditingSettingIndex = -1;
        sEditingColorOffset = -1;
        sSettingsSubMode = sColorReturnSubmode;
    }
}

void handleNameInputInput(uint32_t pressed, uint32_t held)
{
    TextInput::Result result = sInputField.HandleInput(pressed, held);

    if (result == TextInput::Result::CONFIRMED) {
        char name[Settings::MAX_CATEGORY_NAME];
        sInputField.GetValue(name, sizeof(name));

        if (name[0] != '\0') {
            if (sEditingCategoryId < 0) {
                Settings::CreateCategory(name);
            } else {
                Settings::RenameCategory(static_cast<uint16_t>(sEditingCategoryId), name);
            }
            Categories::RefreshFilter();
        }

        sEditingCategoryId = -1;
        sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
    } else if (result == TextInput::Result::CANCELLED) {
        sEditingCategoryId = -1;
        sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
    }
}

void launchSystemApp(int appId)
{
    sIsOpen = false;

    switch (appId) {
        case SYSAPP_RETURN_TO_MENU:
            SYSLaunchMenu();
            break;
        case SYSAPP_BROWSER:
            SYSSwitchToBrowser(nullptr);
            break;
        case SYSAPP_ESHOP:
            SYSSwitchToEShop(nullptr);
            break;
        case SYSAPP_CONTROLLER_SYNC:
            SYSSwitchToSyncControllerOnHBM();
            break;
        default:
            if (appId == SYSTEM_APP_ID_MII_MAKER) {
                SYSLaunchMiiStudio(nullptr);
            } else if (appId == SYSTEM_APP_ID_SYSTEM_SETTINGS) {
                _SYSLaunchSettings(nullptr);
            } else if (appId == SYSTEM_APP_ID_PARENTAL_CONTROLS) {
                _SYSLaunchParental(nullptr);
            } else if (appId == SYSTEM_APP_ID_NOTIFICATIONS) {
                _SYSLaunchNotifications(nullptr);
            } else {
                SYSLaunchMenu();
            }
            break;
    }
}

void handleColorsInput(uint32_t pressed)
{
    UI::ListView::Config listConfig = UI::ListView::InputOnlyConfig(
        Renderer::GetFooterRow() - LIST_START_ROW - 1);
    listConfig.canConfirm = true;
    listConfig.canCancel = true;

    sColorsListState.itemCount = COLOR_OPTION_COUNT;

    UI::ListView::HandleInput(sColorsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sColorsListState);

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    if (action == UI::ListView::Action::CONFIRM && isValidSelection(selectedIdx, COLOR_OPTION_COUNT)) {
        const ColorOption& opt = sColorOptions[selectedIdx];
        sEditingSettingIndex = selectedIdx;
        sEditingColorOffset = opt.dataOffset;
        sColorReturnSubmode = SettingsSubMode::COLORS;
        uint32_t* color = getColorPtr(opt.dataOffset);
        char hexStr[16];
        snprintf(hexStr, sizeof(hexStr), "%08X", *color);
        sInputField.Init(8, TextInput::Library::HEX);
        sInputField.SetValue(hexStr);
        sSettingsSubMode = SettingsSubMode::COLOR_INPUT;
    } else if (action == UI::ListView::Action::CANCEL) {
        sSettingsSubMode = SettingsSubMode::MAIN;
    }
}

void handleSystemAppsInput(uint32_t pressed)
{
    UI::ListView::Config listConfig = UI::ListView::InputOnlyConfig(
        Renderer::GetFooterRow() - LIST_START_ROW - 1);
    listConfig.canConfirm = true;
    listConfig.canCancel = true;

    sSystemAppsListState.itemCount = SYSTEM_APP_COUNT;

    UI::ListView::HandleInput(sSystemAppsListState, pressed, listConfig);

    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::CONFIRM: {
            int idx = UI::ListView::GetSelectedIndex(sSystemAppsListState);
            if (idx >= 0 && idx < SYSTEM_APP_COUNT) {
                launchSystemApp(sSystemApps[idx].appId);
            }
            break;
        }
        case UI::ListView::Action::CANCEL:
            sSettingsSubMode = SettingsSubMode::MAIN;
            break;
        default:
            break;
    }
}

}

void Render()
{
    switch (sSettingsSubMode) {
        case SettingsSubMode::MAIN:
            renderSettingsMain();
            break;
        case SettingsSubMode::MANAGE_CATS:
            renderManageCategories();
            break;
        case SettingsSubMode::SYSTEM_APPS:
            renderSystemApps();
            break;
        case SettingsSubMode::COLORS:
            renderColors();
            break;
        case SettingsSubMode::COLOR_INPUT:
            renderColorInput();
            break;
        case SettingsSubMode::NAME_INPUT:
            renderNameInput();
            break;
    }
}

void HandleInput(uint32_t pressed, uint32_t held)
{
    switch (sSettingsSubMode) {
        case SettingsSubMode::MAIN:
            handleSettingsMainInput(pressed);
            break;
        case SettingsSubMode::MANAGE_CATS:
            handleManageCategoriesInput(pressed);
            break;
        case SettingsSubMode::SYSTEM_APPS:
            handleSystemAppsInput(pressed);
            break;
        case SettingsSubMode::COLORS:
            handleColorsInput(pressed);
            break;
        case SettingsSubMode::COLOR_INPUT:
            handleColorInputInput(pressed, held);
            break;
        case SettingsSubMode::NAME_INPUT:
            handleNameInputInput(pressed, held);
            break;
    }
}

}
}
