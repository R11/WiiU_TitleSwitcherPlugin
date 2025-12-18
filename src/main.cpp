/**
 * Title Switcher Plugin for Wii U (Aroma)
 * Game launcher menu accessible via L + R + Minus button combo.
 */

#include <wups.h>
#include <wups/config_api.h>
#include <vpad/input.h>
#include <notifications/notifications.h>

#include "menu/menu.h"
#include "input/buttons.h"
#include "titles/titles.h"
#include "storage/settings.h"
#include "presets/title_presets.h"

WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Game launcher menu via L+R+Minus");
WUPS_PLUGIN_VERSION("2.0.0");
WUPS_PLUGIN_AUTHOR("R11");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("TitleSwitcher");

static void notify(const char* message)
{
    NotificationModule_AddInfoNotification(message);
}

INITIALIZE_PLUGIN()
{
    NotificationModule_InitLibrary();
    Settings::Init();
    Settings::Load();
    Menu::Init();
    Titles::Load();
    TitlePresets::Load();
    notify("Title Switcher ready");
}

DEINITIALIZE_PLUGIN()
{
    Menu::Shutdown();
    NotificationModule_DeInitLibrary();
}

ON_APPLICATION_START()
{
    Menu::OnApplicationStart();
}

ON_APPLICATION_ENDS()
{
    Menu::OnApplicationEnd();
}

ON_ACQUIRED_FOREGROUND()
{
    Menu::OnForegroundAcquired();
}

ON_RELEASE_FOREGROUND()
{
    Menu::OnForegroundReleased();
}

namespace {

bool comboWasHeldPreviously = false;

void handleInput(VPADStatus* inputBuffer, uint32_t sampleCount)
{
    uint32_t heldButtons = inputBuffer[0].hold;
    bool comboIsHeld = Buttons::IsComboPressed(heldButtons, Buttons::Actions::MENU_OPEN_COMBO);

    if (comboIsHeld && !comboWasHeldPreviously && Menu::IsSafeToOpen()) {
        Menu::Open();

        for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
            inputBuffer[sampleIndex].trigger = 0;
            inputBuffer[sampleIndex].hold = 0;
            inputBuffer[sampleIndex].release = 0;
        }
    }

    comboWasHeldPreviously = comboIsHeld;
}

}

DECL_FUNCTION(int32_t, VPADRead_Game, VPADChan channel, VPADStatus* inputBuffer,
              uint32_t sampleCount, VPADReadError* errorOutput)
{
    VPADReadError readError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Game(channel, inputBuffer, sampleCount, &readError);

    if (result > 0 && readError == VPAD_READ_SUCCESS) {
        handleInput(inputBuffer, sampleCount);
    }

    if (errorOutput) {
        *errorOutput = readError;
    }

    return result;
}

DECL_FUNCTION(int32_t, VPADRead_Menu, VPADChan channel, VPADStatus* inputBuffer,
              uint32_t sampleCount, VPADReadError* errorOutput)
{
    VPADReadError readError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Menu(channel, inputBuffer, sampleCount, &readError);

    if (result > 0 && readError == VPAD_READ_SUCCESS) {
        handleInput(inputBuffer, sampleCount);
    }

    if (errorOutput) {
        *errorOutput = readError;
    }

    return result;
}

WUPS_MUST_REPLACE_FOR_PROCESS(
    VPADRead_Game,
    WUPS_LOADER_LIBRARY_VPAD,
    VPADRead,
    WUPS_FP_TARGET_PROCESS_GAME
);

WUPS_MUST_REPLACE_FOR_PROCESS(
    VPADRead_Menu,
    WUPS_LOADER_LIBRARY_VPAD,
    VPADRead,
    WUPS_FP_TARGET_PROCESS_WII_U_MENU
);
