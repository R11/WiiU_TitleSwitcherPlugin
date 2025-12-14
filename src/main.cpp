/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * Adds game switching to Aroma's config menu (L+DOWN+SELECT).
 * Select a game from the list to launch it.
 *
 * Author: R11
 * License: GPLv3
 */

#include <wups.h>
#include <wups/config_api.h>
#include <notifications/notifications.h>
#include <coreinit/title.h>
#include <coreinit/mcp.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <malloc.h>

// Plugin metadata
WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Switch games from Aroma config menu");
WUPS_PLUGIN_VERSION("0.7.0");
WUPS_PLUGIN_AUTHOR("R11");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("TitleSwitcher");

// ============================================================================
// Configuration
// ============================================================================

#define MAX_TITLES 256

// ============================================================================
// Title Data
// ============================================================================

struct TitleInfo {
    uint64_t titleId;
    char name[64];
};

static TitleInfo sTitles[MAX_TITLES];
static int sTitleCount = 0;
static bool sTitlesLoaded = false;

// Track which game to launch (set by config item callback)
static uint64_t sPendingLaunchTitleId = 0;
static bool sLaunchPending = false;

// ============================================================================
// Debug Helpers
// ============================================================================

static void debugNotify(const char* msg)
{
    NotificationModule_AddInfoNotification(msg);
}

static void debugNotifyF(const char* fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    debugNotify(buf);
}

// ============================================================================
// Title Loading
// ============================================================================

static void getTitleName(uint64_t titleId, char* outName, size_t maxLen)
{
    ACPMetaXml* metaXml = (ACPMetaXml*)memalign(0x40, sizeof(ACPMetaXml));
    if (!metaXml) {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
        return;
    }

    memset(metaXml, 0, sizeof(ACPMetaXml));
    ACPResult result = ACPGetTitleMetaXml(titleId, metaXml);

    if (result == ACP_RESULT_SUCCESS) {
        const char* name = nullptr;
        if (metaXml->shortname_en[0]) name = metaXml->shortname_en;
        else if (metaXml->longname_en[0]) name = metaXml->longname_en;
        else if (metaXml->shortname_ja[0]) name = metaXml->shortname_ja;

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

static bool loadAllTitles()
{
    if (sTitlesLoaded) return sTitleCount > 0;

    sTitleCount = 0;

    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        return false;
    }

    MCPTitleListType* titleList = (MCPTitleListType*)malloc(sizeof(MCPTitleListType) * MAX_TITLES);
    if (!titleList) {
        MCP_Close(mcpHandle);
        return false;
    }

    uint32_t count = 0;
    MCPError err = MCP_TitleListByAppType(mcpHandle, MCP_APP_TYPE_GAME, &count,
                                          titleList, sizeof(MCPTitleListType) * MAX_TITLES);

    if (err >= 0 && count > 0) {
        for (uint32_t i = 0; i < count && sTitleCount < MAX_TITLES; i++) {
            sTitles[sTitleCount].titleId = titleList[i].titleId;
            getTitleName(titleList[i].titleId, sTitles[sTitleCount].name,
                        sizeof(sTitles[sTitleCount].name));
            sTitleCount++;
        }

        // Sort by name
        for (int i = 0; i < sTitleCount - 1; i++) {
            for (int j = 0; j < sTitleCount - i - 1; j++) {
                if (strcasecmp(sTitles[j].name, sTitles[j+1].name) > 0) {
                    TitleInfo temp = sTitles[j];
                    sTitles[j] = sTitles[j+1];
                    sTitles[j+1] = temp;
                }
            }
        }
    }

    free(titleList);
    MCP_Close(mcpHandle);

    sTitlesLoaded = true;
    return sTitleCount > 0;
}

// ============================================================================
// Config Item Callbacks for Game Selection
// ============================================================================

// Get current display text for the item
static int32_t GameItemGetDisplayValue(void* context, char* buf, int32_t bufSize)
{
    snprintf(buf, bufSize, "[Press A to launch]");
    return 0;
}

// Called when item is selected/deselected (cursor moves to/from it)
static void GameItemOnSelected(void* context, bool isSelected)
{
    // Nothing needed
}

// Called to restore default value
static void GameItemRestoreDefault(void* context)
{
    // Nothing needed - no value to restore
}

// Is navigation away from this item allowed
static bool GameItemIsMovementAllowed(void* context)
{
    return true;  // Always allow movement
}

// Called when config menu closes
static void GameItemOnClose(void* context)
{
    // Nothing needed
}

// Called on input - this is where we detect A button to launch
static void GameItemOnInput(void* context, WUPSConfigSimplePadData input)
{
    if (input.buttons_d & WUPS_CONFIG_BUTTON_A) {
        uint64_t* titleIdPtr = (uint64_t*)context;
        if (titleIdPtr) {
            sPendingLaunchTitleId = *titleIdPtr;
            sLaunchPending = true;
        }
    }
}

// Called when item is being destroyed
static void GameItemOnDelete(void* context)
{
    if (context) {
        free(context);
    }
}

// ============================================================================
// Config Menu Callbacks
// ============================================================================

static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
    // Make sure titles are loaded
    if (!sTitlesLoaded) {
        loadAllTitles();
    }

    // Get current running title to exclude it
    uint64_t currentTitleId = OSGetTitleID();

    // Add each game as a config item
    for (int i = 0; i < sTitleCount; i++) {
        // Skip currently running game
        if (sTitles[i].titleId == currentTitleId) {
            continue;
        }

        // Allocate context to hold title ID (freed in OnDelete)
        uint64_t* titleIdContext = (uint64_t*)malloc(sizeof(uint64_t));
        if (!titleIdContext) continue;
        *titleIdContext = sTitles[i].titleId;

        // Create the config item using V2 API
        WUPSConfigAPIItemOptionsV2 itemOptions = {
            .displayName = sTitles[i].name,
            .context = titleIdContext,
            .callbacks = {
                .getCurrentValueDisplay = GameItemGetDisplayValue,
                .getCurrentValueSelectedDisplay = GameItemGetDisplayValue,
                .onSelected = GameItemOnSelected,
                .restoreDefault = GameItemRestoreDefault,
                .isMovementAllowed = GameItemIsMovementAllowed,
                .onCloseCallback = GameItemOnClose,
                .onInput = GameItemOnInput,
                .onInputEx = nullptr,
                .onDelete = GameItemOnDelete,
            }
        };

        WUPSConfigItemHandle itemHandle;
        if (WUPSConfigAPI_Item_Create(itemOptions, &itemHandle) == WUPSCONFIG_API_RESULT_SUCCESS) {
            WUPSConfigAPI_Category_AddItem(rootHandle, itemHandle);
        } else {
            free(titleIdContext);
        }
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void ConfigMenuClosedCallback()
{
    // If a launch is pending, do it now that the menu is closed
    if (sLaunchPending && sPendingLaunchTitleId != 0) {
        sLaunchPending = false;
        uint64_t titleId = sPendingLaunchTitleId;
        sPendingLaunchTitleId = 0;

        // Find the name for notification
        for (int i = 0; i < sTitleCount; i++) {
            if (sTitles[i].titleId == titleId) {
                debugNotifyF("Launching: %s", sTitles[i].name);
                break;
            }
        }

        SYSLaunchTitle(titleId);
    }
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

INITIALIZE_PLUGIN()
{
    NotificationModule_InitLibrary();

    // Initialize the config API
    WUPSConfigAPIOptionsV1 configOptions = {
        .name = "Title Switcher"
    };

    WUPSConfigAPIStatus configStatus = WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);
    if (configStatus != WUPSCONFIG_API_RESULT_SUCCESS) {
        debugNotifyF("Config init failed: %s", WUPSConfigAPI_GetStatusStr(configStatus));
    }

    // Pre-load titles
    loadAllTitles();

    debugNotifyF("Title Switcher: %d games", sTitleCount);
}

DEINITIALIZE_PLUGIN()
{
    NotificationModule_DeInitLibrary();
}
