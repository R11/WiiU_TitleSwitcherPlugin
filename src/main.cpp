/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * Category-based game browser in Aroma config menu.
 * Games organized alphabetically: A-F, G-L, M-R, S-Z, #(numbers/symbols)
 * Select a game and press A to launch.
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
#include <cctype>
#include <malloc.h>

// Plugin metadata
WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Switch games from Aroma config menu");
WUPS_PLUGIN_VERSION("0.9.0");
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

    // Get current title to exclude
    uint64_t currentTitleId = OSGetTitleID();

    uint32_t count = 0;
    MCPError err = MCP_TitleListByAppType(mcpHandle, MCP_APP_TYPE_GAME, &count,
                                          titleList, sizeof(MCPTitleListType) * MAX_TITLES);

    if (err >= 0 && count > 0) {
        for (uint32_t i = 0; i < count && sTitleCount < MAX_TITLES; i++) {
            // Skip current game
            if (titleList[i].titleId == currentTitleId) {
                continue;
            }

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
// Letter Group Helpers
// ============================================================================

// Returns which group (0-4) a character belongs to:
// 0 = A-F, 1 = G-L, 2 = M-R, 3 = S-Z, 4 = # (numbers/symbols)
static int getLetterGroup(char c)
{
    char upper = toupper(c);
    if (upper >= 'A' && upper <= 'F') return 0;
    if (upper >= 'G' && upper <= 'L') return 1;
    if (upper >= 'M' && upper <= 'R') return 2;
    if (upper >= 'S' && upper <= 'Z') return 3;
    return 4; // Numbers, symbols, etc.
}

static const char* getGroupName(int group)
{
    switch (group) {
        case 0: return "A - F";
        case 1: return "G - L";
        case 2: return "M - R";
        case 3: return "S - Z";
        case 4: return "# (0-9)";
        default: return "Other";
    }
}

// ============================================================================
// Config Item Callbacks for Game Selection
// ============================================================================

static int32_t GameItemGetDisplayValue(void* context, char* buf, int32_t bufSize)
{
    snprintf(buf, bufSize, "Press A");
    return 0;
}

static void GameItemOnSelected(void* context, bool isSelected)
{
    // Nothing needed
}

static void GameItemRestoreDefault(void* context)
{
    // Nothing needed
}

static bool GameItemIsMovementAllowed(void* context)
{
    return true;
}

static void GameItemOnClose(void* context)
{
    // Nothing needed
}

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

    if (sTitleCount == 0) {
        // No games - add a placeholder item
        WUPSConfigAPIItemOptionsV2 itemOptions = {
            .displayName = "(No games found)",
            .context = nullptr,
            .callbacks = {
                .getCurrentValueDisplay = [](void*, char* buf, int32_t size) -> int32_t {
                    snprintf(buf, size, "---");
                    return 0;
                },
                .getCurrentValueSelectedDisplay = [](void*, char* buf, int32_t size) -> int32_t {
                    snprintf(buf, size, "---");
                    return 0;
                },
                .onSelected = [](void*, bool) {},
                .restoreDefault = [](void*) {},
                .isMovementAllowed = [](void*) -> bool { return true; },
                .onCloseCallback = [](void*) {},
                .onInput = [](void*, WUPSConfigSimplePadData) {},
                .onInputEx = nullptr,
                .onDelete = [](void*) {},
            }
        };

        WUPSConfigItemHandle itemHandle;
        if (WUPSConfigAPI_Item_Create(itemOptions, &itemHandle) == WUPSCONFIG_API_RESULT_SUCCESS) {
            WUPSConfigAPI_Category_AddItem(rootHandle, itemHandle);
        }
        return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
    }

    // Count games per group to know which categories to create
    int groupCounts[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < sTitleCount; i++) {
        int group = getLetterGroup(sTitles[i].name[0]);
        groupCounts[group]++;
    }

    // Create categories for each non-empty group
    for (int g = 0; g < 5; g++) {
        if (groupCounts[g] == 0) continue;

        // Create category name with count
        char categoryName[32];
        snprintf(categoryName, sizeof(categoryName), "%s (%d)", getGroupName(g), groupCounts[g]);

        WUPSConfigCategoryHandle groupCategory;
        if (WUPSConfigAPI_Category_Create({.name = categoryName}, &groupCategory) != WUPSCONFIG_API_RESULT_SUCCESS) {
            continue;
        }

        // Add games in this group
        for (int i = 0; i < sTitleCount; i++) {
            int titleGroup = getLetterGroup(sTitles[i].name[0]);
            if (titleGroup != g) continue;

            // Allocate context to hold title ID (freed in OnDelete)
            uint64_t* titleIdContext = (uint64_t*)malloc(sizeof(uint64_t));
            if (!titleIdContext) continue;
            *titleIdContext = sTitles[i].titleId;

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
                WUPSConfigAPI_Category_AddItem(groupCategory, itemHandle);
            } else {
                free(titleIdContext);
            }
        }

        WUPSConfigAPI_Category_AddCategory(rootHandle, groupCategory);
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void ConfigMenuClosedCallback()
{
    // If a launch is pending, do it now
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

    WUPSConfigAPIStatus configStatus = WUPSConfigAPI_Init(configOptions,
        ConfigMenuOpenedCallback, ConfigMenuClosedCallback);

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
