/**
 * Main Menu Implementation
 * Core lifecycle, state management, and mode dispatching.
 */

#include "menu.h"
#include "menu_state.h"
#include "categories.h"
#include "panels/browse_panel.h"
#include "panels/settings_panel.h"
#include "panels/edit_panel.h"
#include "panels/debug_panel.h"
#include "../render/renderer.h"
#include "../render/image_loader.h"
#include "../render/measurements.h"
#include "../input/buttons.h"
#include "../titles/titles.h"
#include "../storage/settings.h"
#include "../ui/list_view.h"

#include <vpad/input.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <coreinit/time.h>
#include <notifications/notifications.h>

#include <cstdio>
#include <cstring>

namespace Menu {

namespace Internal {

bool sIsOpen = false;
bool sInitialized = false;
Mode sCurrentMode = Mode::BROWSE;

UI::ListView::State sTitleListState;
UI::ListView::State sEditCatsListState;
UI::ListView::State sSettingsListState;
UI::ListView::State sManageCatsListState;
UI::ListView::State sSystemAppsListState;
UI::ListView::State sColorsListState;

SettingsSubMode sSettingsSubMode = SettingsSubMode::MAIN;
SettingsSubMode sColorReturnSubmode = SettingsSubMode::MAIN;
int sEditingSettingIndex = -1;
int sEditingColorOffset = -1;
int sEditingCategoryId = -1;
TextInput::Field sInputField;

OSTime sApplicationStartTime = 0;
bool sInForeground = false;
bool sOpeningInProgress = false;
const uint32_t STARTUP_GRACE_MS = 3000;

const SettingItem sSettingItems[] = {
    ACTION_SETTING("System Apps",       "Launch system applications",   "(Browser, Settings, etc.)", ACTION_SYSTEM_APPS),
    TOGGLE_SETTING("Show Numbers",      "Show line numbers before",     "each title in the list.",   showNumbers),
    TOGGLE_SETTING("Show Favorites",    "Show favorite marker (*)",     "in the title list.",        showFavorites),
    ACTION_SETTING("Customize Colors",  "Change menu colors:",          "background, text, etc.",    ACTION_COLORS),
    ACTION_SETTING("Manage Categories", "Create, rename, or delete",    "custom categories.",        ACTION_MANAGE_CATEGORIES),
    ACTION_SETTING("Debug Grid",        "Show grid overlay with",       "dimensions and positions.", ACTION_DEBUG_GRID),
};
const int SETTINGS_ITEM_COUNT = sizeof(sSettingItems) / sizeof(sSettingItems[0]);

const ColorOption sColorOptions[] = {
    COLOR_OPTION("Background",        bgColor),
    COLOR_OPTION("Title Text",        titleColor),
    COLOR_OPTION("Highlighted Title", highlightedTitleColor),
    COLOR_OPTION("Favorite Marker",   favoriteColor),
    COLOR_OPTION("Header Text",       headerColor),
    COLOR_OPTION("Category Text",     categoryColor),
};
const int COLOR_OPTION_COUNT = sizeof(sColorOptions) / sizeof(sColorOptions[0]);

const SystemAppOption sSystemApps[] = {
    {"Return to Menu",       "Exit game and return to Wii U Menu",         SYSAPP_RETURN_TO_MENU},
    {"Internet Browser",     "Open the Internet Browser",                   SYSAPP_BROWSER},
    {"Nintendo eShop",       "Open the Nintendo eShop",                     SYSAPP_ESHOP},
    {"Mii Maker",            "Open Mii Maker",                              SYSTEM_APP_ID_MII_MAKER},
    {"System Settings",      "Open System Settings",                        SYSTEM_APP_ID_SYSTEM_SETTINGS},
    {"Parental Controls",    "Open Parental Controls",                      SYSTEM_APP_ID_PARENTAL_CONTROLS},
    {"Daily Log",            "View play activity",                          SYSTEM_APP_ID_DAILY_LOG},
};
const int SYSTEM_APP_COUNT = sizeof(sSystemApps) / sizeof(sSystemApps[0]);

bool* getTogglePtr(int offset)
{
    return (bool*)((char*)&Settings::Get() + offset);
}

uint32_t* getColorPtr(int offset)
{
    return (uint32_t*)((char*)&Settings::Get() + offset);
}

void clampSelection()
{
    int count = Categories::GetFilteredCount();
    sTitleListState.SetItemCount(count, Renderer::GetVisibleRows());
}

void drawHeaderDivider()
{
    Renderer::DrawText(0, HEADER_ROW, Measurements::HEADER_DIVIDER);
}

void drawDetailsPanelSectionHeader(const char* title, bool shortUnderline)
{
    int col = Renderer::GetDetailsPanelCol();
    Renderer::DrawText(col, LIST_START_ROW, title, Settings::Get().headerColor);
    Renderer::DrawText(col, LIST_START_ROW + Measurements::ROW_OFFSET_UNDERLINE,
                      shortUnderline ? Measurements::SECTION_UNDERLINE_SHORT : Measurements::SECTION_UNDERLINE);
}

const char* getSettingActionHint(SettingType type)
{
    switch (type) {
        case SettingType::TOGGLE:
            return "A: Toggle value";
        case SettingType::COLOR:
            return "A: Edit color";
        case SettingType::BRIGHTNESS:
            return "A: Cycle brightness";
        case SettingType::ACTION:
            return "A: Select";
        default:
            return "";
    }
}

}

using namespace Internal;

namespace {

FrameResult processFrameInternal()
{
    FrameResult result = {true, 0};

    if (!sIsOpen) {
        result.shouldContinue = false;
        return result;
    }

    Renderer::BeginFrame(Settings::Get().bgColor);

    switch (sCurrentMode) {
        case Mode::BROWSE:
            BrowsePanel::Render();
            break;
        case Mode::EDIT:
            EditPanel::Render();
            break;
        case Mode::SETTINGS:
            SettingsPanel::Render();
            break;
        case Mode::DEBUG_GRID:
            DebugPanel::Render();
            break;
    }

    static uint32_t frameCounter = 0;
    frameCounter++;
    if (ImageLoader::HasHighPriorityPending() || frameCounter % 10 == 0) {
        ImageLoader::Update();
    }

    Renderer::EndFrame();

    VPADStatus vpadStatus;
    VPADReadError vpadError;
    int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

    if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
        uint32_t pressed = vpadStatus.trigger;
        uint32_t held = vpadStatus.hold;

        switch (sCurrentMode) {
            case Mode::BROWSE:
                result.titleToLaunch = BrowsePanel::HandleInput(pressed);
                break;
            case Mode::EDIT:
                EditPanel::HandleInput(pressed);
                break;
            case Mode::SETTINGS:
                SettingsPanel::HandleInput(pressed, held);
                break;
            case Mode::DEBUG_GRID:
                DebugPanel::HandleInput(pressed);
                break;
        }
    }

    if (!sIsOpen) {
        result.shouldContinue = false;
    }

    return result;
}

uint64_t runMenuLoop()
{
    while (sIsOpen) {
        FrameResult result = processFrameInternal();
        if (!result.shouldContinue) {
            return result.titleToLaunch;
        }
    }
    return 0;
}

}

void Init()
{
    sInitialized = true;
    sIsOpen = false;
    sCurrentMode = Mode::BROWSE;
}

void Shutdown()
{
    if (sIsOpen) {
        Close();
    }
    sInitialized = false;
}

bool IsOpen()
{
    return sIsOpen;
}

bool IsSafeToOpen()
{
    if (sIsOpen) {
        return false;
    }

    if (sApplicationStartTime != 0) {
        OSTime now = OSGetTime();
        OSTime elapsed = now - sApplicationStartTime;
        uint32_t elapsedMs = static_cast<uint32_t>(OSTicksToMilliseconds(elapsed));

        if (elapsedMs < STARTUP_GRACE_MS) {
            return false;
        }
    }

    if (!sInForeground) {
        return false;
    }

    return true;
}

Mode GetMode()
{
    return sCurrentMode;
}

void Open()
{
    if (sIsOpen) return;

    sOpeningInProgress = true;

    if (!Renderer::Init()) {
        NotificationModule_AddErrorNotification("Menu unavailable - not enough memory");
        sOpeningInProgress = false;
        return;
    }

    Titles::Load();
    Categories::Init();
    ImageLoader::RetryFailed();

    sTitleListState = UI::ListView::State();
    sTitleListState.selectedIndex = Settings::Get().lastIndex;
    clampSelection();

    sIsOpen = true;
    sOpeningInProgress = false;
    sCurrentMode = Mode::BROWSE;

    uint64_t titleToLaunch = runMenuLoop();

    Settings::Get().lastIndex = sTitleListState.selectedIndex;
    Settings::Save();

    sIsOpen = false;
    Renderer::Shutdown();

    if (titleToLaunch != 0) {
        SYSLaunchTitle(titleToLaunch);
    }
}

void Close()
{
    sIsOpen = false;
}

FrameResult ProcessFrame()
{
    return processFrameInternal();
}

void RenderFrame()
{
    if (!sIsOpen) return;

    Renderer::BeginFrame(Settings::Get().bgColor);

    switch (sCurrentMode) {
        case Mode::BROWSE:
            BrowsePanel::Render();
            break;
        case Mode::EDIT:
            EditPanel::Render();
            break;
        case Mode::SETTINGS:
            SettingsPanel::Render();
            break;
        case Mode::DEBUG_GRID:
            DebugPanel::Render();
            break;
    }
}

FrameResult HandleInputFrame()
{
    FrameResult result = {true, 0};

    if (!sIsOpen) {
        result.shouldContinue = false;
        return result;
    }

    static uint32_t frameCounter = 0;
    frameCounter++;
    if (ImageLoader::HasHighPriorityPending() || frameCounter % 10 == 0) {
        ImageLoader::Update();
    }

    VPADStatus vpadStatus;
    VPADReadError vpadError;
    int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

    if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
        uint32_t pressed = vpadStatus.trigger;
        uint32_t held = vpadStatus.hold;

        switch (sCurrentMode) {
            case Mode::BROWSE:
                result.titleToLaunch = BrowsePanel::HandleInput(pressed);
                break;
            case Mode::EDIT:
                EditPanel::HandleInput(pressed);
                break;
            case Mode::SETTINGS:
                SettingsPanel::HandleInput(pressed, held);
                break;
            case Mode::DEBUG_GRID:
                DebugPanel::HandleInput(pressed);
                break;
        }
    }

    if (!sIsOpen) {
        result.shouldContinue = false;
    }

    return result;
}

void ResetToBrowse()
{
    sCurrentMode = Mode::BROWSE;
    sIsOpen = true;
}

void InitForWebPreview()
{
    sInitialized = true;
    sIsOpen = true;
    sCurrentMode = Mode::BROWSE;
    sInForeground = true;
    sTitleListState = UI::ListView::State();
    clampSelection();
}

void OnApplicationStart()
{
    sApplicationStartTime = OSGetTime();
    sInForeground = true;
}

void OnApplicationEnd()
{
    sApplicationStartTime = 0;
    sInForeground = false;

    if (sIsOpen) {
        Close();
    }
}

void OnForegroundAcquired()
{
    sInForeground = true;
}

void OnForegroundReleased()
{
    if (sOpeningInProgress) {
        return;
    }

    sInForeground = false;

    if (sIsOpen) {
        Close();
    }
}

}
