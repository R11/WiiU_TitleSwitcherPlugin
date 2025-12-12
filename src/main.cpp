/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * Allows switching between games directly from the HOME button menu
 * without returning to the Wii U Menu.
 *
 * Button combo: L + R + MINUS (SELECT) while in HOME menu to activate
 * Then: UP/DOWN to navigate, A to launch, B to cancel
 *
 * Author: Rudger
 * License: GPLv3
 */

#include <wups.h>
#include <notifications/notifications.h>
#include <vpad/input.h>
#include <coreinit/debug.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>
#include <coreinit/mcp.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>
#include <cstring>
#include <cstdio>
#include <malloc.h>
#include <vector>
#include <algorithm>

// Plugin metadata
WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Switch between games from the HOME menu");
WUPS_PLUGIN_VERSION("0.2.0");
WUPS_PLUGIN_AUTHOR("Rudger");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();

// Button combo to trigger title switcher: L + R + MINUS
#define TRIGGER_COMBO (VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS)

// Maximum number of titles
#define MAX_TITLES 100

// Track states
static bool sNotificationsReady = false;
static bool sMenuActive = false;

// Structure to hold title information
struct TitleInfo {
    uint64_t titleId;
    char name[128];
};

// Storage for enumerated titles
static std::vector<TitleInfo> sTitles;
static int sSelectedIndex = 0;

/**
 * Initialize notifications
 */
static void initNotifications()
{
    NotificationModuleStatus status = NotificationModule_InitLibrary();
    if (status != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        OSReport("[TitleSwitcher] Failed to init notifications: %d\n", status);
        sNotificationsReady = false;
        return;
    }

    NotificationModule_SetDefaultValue(
        NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
        NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT,
        3.0f
    );

    sNotificationsReady = true;
}

/**
 * Show notification
 */
static void showNotification(const char* message)
{
    if (sNotificationsReady) {
        NotificationModule_AddInfoNotification(message);
    }
    OSReport("[TitleSwitcher] %s\n", message);
}

/**
 * Get title name from metadata
 */
static void getTitleName(uint64_t titleId, char* outName, size_t maxLen)
{
    // Allocate aligned buffer for ACPMetaXml (must be 0x40 aligned)
    ACPMetaXml* metaXml = (ACPMetaXml*)memalign(0x40, sizeof(ACPMetaXml));
    if (!metaXml) {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
        return;
    }

    memset(metaXml, 0, sizeof(ACPMetaXml));

    ACPResult result = ACPGetTitleMetaXml(titleId, metaXml);
    if (result == ACP_RESULT_SUCCESS) {
        const char* name = nullptr;

        if (metaXml->shortname_en[0] != '\0') {
            name = metaXml->shortname_en;
        } else if (metaXml->longname_en[0] != '\0') {
            name = metaXml->longname_en;
        } else if (metaXml->shortname_ja[0] != '\0') {
            name = metaXml->shortname_ja;
        }

        if (name) {
            snprintf(outName, maxLen, "%s", name);
        } else {
            snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
        }
    } else {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
    }

    free(metaXml);
}

/**
 * Enumerate all installed game titles
 */
static int enumerateTitles()
{
    sTitles.clear();

    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        OSReport("[TitleSwitcher] Failed to open MCP: %d\n", mcpHandle);
        return 0;
    }

    MCPTitleListType* titleList = (MCPTitleListType*)malloc(sizeof(MCPTitleListType) * MAX_TITLES);
    if (!titleList) {
        OSReport("[TitleSwitcher] Failed to allocate title list\n");
        MCP_Close(mcpHandle);
        return 0;
    }

    uint32_t titleCount = 0;
    MCPError err = MCP_TitleListByAppType(mcpHandle, MCP_APP_TYPE_GAME, &titleCount,
                                          titleList, sizeof(MCPTitleListType) * MAX_TITLES);

    if (err >= 0 && titleCount > 0) {
        OSReport("[TitleSwitcher] Found %u game titles\n", titleCount);

        uint64_t currentTitleId = OSGetTitleID();

        for (uint32_t i = 0; i < titleCount && sTitles.size() < MAX_TITLES; i++) {
            // Skip the currently running title
            if (titleList[i].titleId == currentTitleId) {
                continue;
            }

            TitleInfo info;
            info.titleId = titleList[i].titleId;
            getTitleName(titleList[i].titleId, info.name, sizeof(info.name));

            sTitles.push_back(info);
        }

        // Sort alphabetically
        std::sort(sTitles.begin(), sTitles.end(), [](const TitleInfo& a, const TitleInfo& b) {
            return strcasecmp(a.name, b.name) < 0;
        });
    }

    free(titleList);
    MCP_Close(mcpHandle);

    OSReport("[TitleSwitcher] Enumerated %zu titles\n", sTitles.size());
    return (int)sTitles.size();
}

/**
 * Show current selection via notification
 */
static void showCurrentSelection()
{
    if (sTitles.empty()) {
        showNotification("No games found!");
        return;
    }

    char msg[192];
    snprintf(msg, sizeof(msg), "[%d/%zu] %s",
             sSelectedIndex + 1,
             sTitles.size(),
             sTitles[sSelectedIndex].name);
    showNotification(msg);
}

/**
 * Launch the selected title
 */
static void launchSelectedTitle()
{
    if (sSelectedIndex >= 0 && sSelectedIndex < (int)sTitles.size()) {
        uint64_t titleId = sTitles[sSelectedIndex].titleId;
        const char* name = sTitles[sSelectedIndex].name;

        char msg[192];
        snprintf(msg, sizeof(msg), "Launching: %s", name);
        showNotification(msg);

        OSReport("[TitleSwitcher] Launching title %016llX (%s)\n",
                 (unsigned long long)titleId, name);

        OSEnableHomeButtonMenu(FALSE);
        SYSLaunchTitle(titleId);
    }
}

/**
 * Activate the menu
 */
static void activateMenu()
{
    if (sMenuActive) return;

    OSReport("[TitleSwitcher] Activating menu...\n");

    int count = enumerateTitles();
    if (count == 0) {
        showNotification("No games found!");
        return;
    }

    sSelectedIndex = 0;
    sMenuActive = true;

    char msg[64];
    snprintf(msg, sizeof(msg), "Title Switcher: %d games", count);
    showNotification(msg);

    // Show first selection
    showCurrentSelection();

    showNotification("UP/DOWN: Navigate | A: Launch | B: Cancel");
}

/**
 * Deactivate the menu
 */
static void deactivateMenu()
{
    if (!sMenuActive) return;

    OSReport("[TitleSwitcher] Deactivating menu...\n");
    sMenuActive = false;
    sTitles.clear();
    showNotification("Title Switcher closed");
}

INITIALIZE_PLUGIN()
{
    OSReport("[TitleSwitcher] Plugin initializing...\n");
    initNotifications();
    OSReport("[TitleSwitcher] Plugin initialized!\n");
}

DEINITIALIZE_PLUGIN()
{
    OSReport("[TitleSwitcher] Plugin deinitializing...\n");
    sMenuActive = false;
    sTitles.clear();
    if (sNotificationsReady) {
        NotificationModule_DeInitLibrary();
    }
}

DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus *buffer, uint32_t bufferSize, VPADReadError *error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        uint32_t pressed = buffer[0].trigger;

        if (sMenuActive) {
            // Menu is active - handle navigation

            // Cancel with B
            if (pressed & VPAD_BUTTON_B) {
                deactivateMenu();
            }
            // Launch with A
            else if (pressed & VPAD_BUTTON_A) {
                sMenuActive = false;
                launchSelectedTitle();
                sTitles.clear();
            }
            // Navigate up
            else if (pressed & VPAD_BUTTON_UP) {
                if (!sTitles.empty()) {
                    sSelectedIndex--;
                    if (sSelectedIndex < 0) {
                        sSelectedIndex = (int)sTitles.size() - 1; // Wrap around
                    }
                    showCurrentSelection();
                }
            }
            // Navigate down
            else if (pressed & VPAD_BUTTON_DOWN) {
                if (!sTitles.empty()) {
                    sSelectedIndex++;
                    if (sSelectedIndex >= (int)sTitles.size()) {
                        sSelectedIndex = 0; // Wrap around
                    }
                    showCurrentSelection();
                }
            }

            // Block navigation buttons from reaching HOME menu while active
            if (pressed & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_A | VPAD_BUTTON_B)) {
                for (uint32_t i = 0; i < bufferSize; i++) {
                    buffer[i].trigger &= ~(VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_A | VPAD_BUTTON_B);
                    buffer[i].hold &= ~(VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_A | VPAD_BUTTON_B);
                    buffer[i].release &= ~(VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_A | VPAD_BUTTON_B);
                }
            }
        } else {
            // Menu not active - check for activation combo
            if ((pressed & TRIGGER_COMBO) == TRIGGER_COMBO) {
                if (OSIsHomeButtonMenuEnabled()) {
                    activateMenu();
                }
            }
        }
    }

    if (error) {
        *error = realError;
    }

    return result;
}

WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_HOME_MENU);
