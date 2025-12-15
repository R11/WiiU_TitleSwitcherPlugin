/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * Category-based game browser in Aroma config menu.
 * - Games organized alphabetically: A-F, G-L, M-R, S-Z, # (numbers/symbols)
 * - System Apps in separate category
 * - Select a game and press A to launch
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
#include <vpad/input.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <malloc.h>

#include "custom_menu.h"

// Plugin metadata
WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Switch games via config menu or L+R+Minus");
WUPS_PLUGIN_VERSION("1.1.0");
WUPS_PLUGIN_AUTHOR("R11");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("TitleSwitcher");

// ============================================================================
// Constants
// ============================================================================

#define MAX_TITLES 512

// System App Title IDs (manually added since MCP may not return them)
#define VWII_TITLE_ID           0x0005001010004000ULL  // cafe2wii - launches vWii mode
#define SYSTEM_SETTINGS_JPN     0x0005001010047000ULL
#define SYSTEM_SETTINGS_USA     0x0005001010047100ULL
#define SYSTEM_SETTINGS_EUR     0x0005001010047200ULL
#define MII_MAKER_JPN           0x000500101004A000ULL
#define MII_MAKER_USA           0x000500101004A100ULL
#define MII_MAKER_EUR           0x000500101004A200ULL
#define ESHOP_JPN               0x0005001010036000ULL
#define ESHOP_USA               0x0005001010036100ULL
#define ESHOP_EUR               0x0005001010036200ULL

// Category indices
enum Category {
    CAT_A_F = 0,
    CAT_G_L,
    CAT_M_R,
    CAT_S_Z,
    CAT_NUMBERS,
    CAT_SYSTEM,
    CAT_COUNT
};

static const char* sCategoryNames[CAT_COUNT] = {
    "A - F",
    "G - L",
    "M - R",
    "S - Z",
    "# (0-9)",
    "System Apps"
};

// ============================================================================
// Title Data
// ============================================================================

struct TitleInfo {
    uint64_t titleId;
    char name[64];
    Category category;
};

static TitleInfo sTitles[MAX_TITLES];
static int sTitleCount = 0;
static bool sTitlesLoaded = false;

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
// Category Helpers
// ============================================================================

static Category getLetterCategory(char c)
{
    char upper = toupper(c);
    if (upper >= 'A' && upper <= 'F') return CAT_A_F;
    if (upper >= 'G' && upper <= 'L') return CAT_G_L;
    if (upper >= 'M' && upper <= 'R') return CAT_M_R;
    if (upper >= 'S' && upper <= 'Z') return CAT_S_Z;
    return CAT_NUMBERS;
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
            snprintf(outName, maxLen, "%.63s", name);  // Limit to 63 chars + null
        } else {
            snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
        }
    } else {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
    }

    free(metaXml);
}

// Add a single title to the list
static void addTitle(uint64_t titleId, Category category, const char* customName = nullptr)
{
    if (sTitleCount >= MAX_TITLES) return;

    sTitles[sTitleCount].titleId = titleId;
    sTitles[sTitleCount].category = category;

    if (customName) {
        snprintf(sTitles[sTitleCount].name, sizeof(sTitles[sTitleCount].name), "%s", customName);
    } else {
        getTitleName(titleId, sTitles[sTitleCount].name, sizeof(sTitles[sTitleCount].name));
    }

    // For non-system titles, determine category from name if not specified
    if (category != CAT_SYSTEM) {
        sTitles[sTitleCount].category = getLetterCategory(sTitles[sTitleCount].name[0]);
    }

    sTitleCount++;
}

static void loadAllTitles()
{
    if (sTitlesLoaded) return;

    sTitleCount = 0;
    uint64_t currentTitleId = OSGetTitleID();

    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        sTitlesLoaded = true;
        return;
    }

    MCPTitleListType* titleList = (MCPTitleListType*)malloc(sizeof(MCPTitleListType) * MAX_TITLES);
    if (!titleList) {
        MCP_Close(mcpHandle);
        sTitlesLoaded = true;
        return;
    }

    // 1. Load Wii U Games
    uint32_t count = 0;
    MCPError err = MCP_TitleListByAppType(mcpHandle, MCP_APP_TYPE_GAME, &count,
                                          titleList, sizeof(MCPTitleListType) * MAX_TITLES);
    if (err >= 0 && count > 0) {
        for (uint32_t i = 0; i < count; i++) {
            if (titleList[i].titleId == currentTitleId) continue;
            addTitle(titleList[i].titleId, CAT_A_F);  // Category will be determined by name
        }
    }

    // 2. Manually add System Apps (USA region)
    addTitle(VWII_TITLE_ID, CAT_SYSTEM, "vWii Mode");
    addTitle(SYSTEM_SETTINGS_USA, CAT_SYSTEM, "System Settings");
    addTitle(MII_MAKER_USA, CAT_SYSTEM, "Mii Maker");
    addTitle(ESHOP_USA, CAT_SYSTEM, "Nintendo eShop");

    free(titleList);
    MCP_Close(mcpHandle);

    // Sort all titles alphabetically by name
    // Menu building groups by category, so this ensures alphabetical order within each
    for (int i = 0; i < sTitleCount - 1; i++) {
        for (int j = 0; j < sTitleCount - i - 1; j++) {
            if (strcasecmp(sTitles[j].name, sTitles[j+1].name) > 0) {
                TitleInfo temp = sTitles[j];
                sTitles[j] = sTitles[j+1];
                sTitles[j+1] = temp;
            }
        }
    }

    sTitlesLoaded = true;
}

// ============================================================================
// Config Item Callbacks
// ============================================================================

static int32_t GameItemGetDisplayValue(void* context, char* buf, int32_t bufSize)
{
    snprintf(buf, bufSize, "Press A");
    return 0;
}

static void GameItemOnInput(void* context, WUPSConfigSimplePadData input)
{
    if (input.buttons_d & WUPS_CONFIG_BUTTON_A) {
        uint64_t* titleIdPtr = (uint64_t*)context;
        if (titleIdPtr) {
            uint64_t titleId = *titleIdPtr;

            // Find name for notification
            for (int i = 0; i < sTitleCount; i++) {
                if (sTitles[i].titleId == titleId) {
                    debugNotifyF("Launching: %s", sTitles[i].name);
                    break;
                }
            }

            SYSLaunchTitle(titleId);
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
// Config Menu Building
// ============================================================================

static void addGameItemToCategory(WUPSConfigCategoryHandle category, TitleInfo* title)
{
    // Allocate context to hold title ID (freed in OnDelete)
    uint64_t* titleIdContext = (uint64_t*)malloc(sizeof(uint64_t));
    if (!titleIdContext) return;
    *titleIdContext = title->titleId;

    WUPSConfigAPIItemOptionsV2 itemOptions = {
        .displayName = title->name,
        .context = titleIdContext,
        .callbacks = {
            .getCurrentValueDisplay = GameItemGetDisplayValue,
            .getCurrentValueSelectedDisplay = GameItemGetDisplayValue,
            .onSelected = [](void*, bool) {},
            .restoreDefault = [](void*) {},
            .isMovementAllowed = [](void*) -> bool { return true; },
            .onCloseCallback = [](void*) {},
            .onInput = GameItemOnInput,
            .onInputEx = nullptr,
            .onDelete = GameItemOnDelete,
        }
    };

    WUPSConfigItemHandle itemHandle;
    if (WUPSConfigAPI_Item_Create(itemOptions, &itemHandle) == WUPSCONFIG_API_RESULT_SUCCESS) {
        WUPSConfigAPI_Category_AddItem(category, itemHandle);
    } else {
        free(titleIdContext);
    }
}

static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
    if (!sTitlesLoaded) {
        loadAllTitles();
    }

    if (sTitleCount == 0) {
        // No titles - show placeholder
        WUPSConfigAPIItemOptionsV2 itemOptions = {
            .displayName = "(No titles found)",
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

    // Category handles and tracking
    WUPSConfigCategoryHandle categories[CAT_COUNT];
    bool categoryCreated[CAT_COUNT] = {false};
    int categoryCounts[CAT_COUNT] = {0};

    // First pass: count titles per category
    for (int i = 0; i < sTitleCount; i++) {
        categoryCounts[sTitles[i].category]++;
    }

    // Create categories that have titles
    for (int c = 0; c < CAT_COUNT; c++) {
        if (categoryCounts[c] == 0) continue;

        char categoryName[48];
        snprintf(categoryName, sizeof(categoryName), "%s (%d)", sCategoryNames[c], categoryCounts[c]);

        if (WUPSConfigAPI_Category_Create({.name = categoryName}, &categories[c]) == WUPSCONFIG_API_RESULT_SUCCESS) {
            WUPSConfigAPI_Category_AddCategory(rootHandle, categories[c]);
            categoryCreated[c] = true;
        }
    }

    // Second pass: add titles to their categories
    for (int i = 0; i < sTitleCount; i++) {
        Category cat = sTitles[i].category;
        if (categoryCreated[cat]) {
            addGameItemToCategory(categories[cat], &sTitles[i]);
        }
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void ConfigMenuClosedCallback()
{
    // Nothing needed - we launch immediately on A press now
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

INITIALIZE_PLUGIN()
{
    NotificationModule_InitLibrary();

    WUPSConfigAPIOptionsV1 configOptions = {
        .name = "Title Switcher"
    };

    WUPSConfigAPIStatus configStatus = WUPSConfigAPI_Init(configOptions,
        ConfigMenuOpenedCallback, ConfigMenuClosedCallback);

    if (configStatus != WUPSCONFIG_API_RESULT_SUCCESS) {
        debugNotifyF("Config init failed: %s", WUPSConfigAPI_GetStatusStr(configStatus));
    }

    // Initialize custom menu system and preload titles for instant menu open
    CustomMenu::Init();
    CustomMenu::PreloadTitles();

    // Pre-load titles for config menu
    loadAllTitles();

    debugNotifyF("Title Switcher: %d titles", sTitleCount);
}

DEINITIALIZE_PLUGIN()
{
    CustomMenu::Shutdown();
    NotificationModule_DeInitLibrary();
}

// ============================================================================
// Custom Menu Button Combo Hook (L + R + Minus)
// ============================================================================

#define CUSTOM_MENU_COMBO (VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS)

static bool sComboWasHeld = false;

static void handleCustomMenuInput(VPADStatus *buffer, uint32_t bufferSize)
{
    uint32_t held = buffer[0].hold;

    // Check for L + R + Minus combo
    bool comboHeld = (held & CUSTOM_MENU_COMBO) == CUSTOM_MENU_COMBO;

    // Trigger on press (not held from previous frame)
    if (comboHeld && !sComboWasHeld && !CustomMenu::IsOpen()) {
        // Open custom menu - this blocks until menu closes
        CustomMenu::Open();

        // Clear the input buffer so game/menu doesn't see the combo
        for (uint32_t i = 0; i < bufferSize; i++) {
            buffer[i].trigger = 0;
            buffer[i].hold = 0;
            buffer[i].release = 0;
        }
    }

    sComboWasHeld = comboHeld;
}

// Hook for GAME processes
DECL_FUNCTION(int32_t, VPADRead_Game, VPADChan chan, VPADStatus *buffer, uint32_t bufferSize, VPADReadError *error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Game(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleCustomMenuInput(buffer, bufferSize);
    }

    if (error) *error = realError;
    return result;
}

// Hook for Wii U Menu
DECL_FUNCTION(int32_t, VPADRead_Menu, VPADChan chan, VPADStatus *buffer, uint32_t bufferSize, VPADReadError *error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Menu(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleCustomMenuInput(buffer, bufferSize);
    }

    if (error) *error = realError;
    return result;
}

WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead_Game, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_GAME);
WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead_Menu, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
