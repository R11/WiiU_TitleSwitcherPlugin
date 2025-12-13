/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * Shows game selection overlay while in a game (not HOME menu).
 * Press TV button to open, navigate with D-pad, A to launch, B to cancel.
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
#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/swap.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>
#include <cstring>
#include <cstdio>
#include <malloc.h>

// Plugin metadata
WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Switch between games with TV button");
WUPS_PLUGIN_VERSION("0.5.0");
WUPS_PLUGIN_AUTHOR("Rudger");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();

// Trigger button - TV button for testing
#define TRIGGER_BUTTON VPAD_BUTTON_TV

// Maximum titles
#define MAX_TITLES 64
#define VISIBLE_TITLES 12

// Title info
struct TitleInfo {
    uint64_t titleId;
    char name[64];
};

static TitleInfo sTitles[MAX_TITLES];
static int sTitleCount = 0;

// Menu state
static bool sMenuOpen = false;
static int sSelectedIndex = 0;
static int sScrollOffset = 0;

// Screen buffers
static void* sScreenBufferTV = nullptr;
static void* sScreenBufferDRC = nullptr;
static uint32_t sScreenSizeTV = 0;
static uint32_t sScreenSizeDRC = 0;
static bool sScreenInitialized = false;

/**
 * Get title name from metadata
 */
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

/**
 * Load game list
 */
static bool loadTitles()
{
    sTitleCount = 0;

    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        OSReport("[TitleSwitcher] MCP_Open failed: %d\n", mcpHandle);
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
        uint64_t currentId = OSGetTitleID();

        for (uint32_t i = 0; i < count && sTitleCount < MAX_TITLES; i++) {
            if (titleList[i].titleId == currentId) continue;

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

    OSReport("[TitleSwitcher] Loaded %d titles\n", sTitleCount);
    return sTitleCount > 0;
}

/**
 * Initialize OSScreen
 */
static bool initScreen()
{
    if (sScreenInitialized) return true;

    OSScreenInit();

    sScreenSizeTV = OSScreenGetBufferSizeEx(SCREEN_TV);
    sScreenSizeDRC = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // Use memalign for framebuffer memory - must be 0x100 aligned
    sScreenBufferTV = memalign(0x100, sScreenSizeTV);
    sScreenBufferDRC = memalign(0x100, sScreenSizeDRC);

    if (!sScreenBufferTV || !sScreenBufferDRC) {
        if (sScreenBufferTV) free(sScreenBufferTV);
        if (sScreenBufferDRC) free(sScreenBufferDRC);
        sScreenBufferTV = nullptr;
        sScreenBufferDRC = nullptr;
        return false;
    }

    // Zero out the buffers first
    memset(sScreenBufferTV, 0, sScreenSizeTV);
    memset(sScreenBufferDRC, 0, sScreenSizeDRC);
    DCFlushRange(sScreenBufferTV, sScreenSizeTV);
    DCFlushRange(sScreenBufferDRC, sScreenSizeDRC);

    OSScreenSetBufferEx(SCREEN_TV, sScreenBufferTV);
    OSScreenSetBufferEx(SCREEN_DRC, sScreenBufferDRC);
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    sScreenInitialized = true;
    return true;
}

/**
 * Shutdown OSScreen
 */
static void shutdownScreen()
{
    if (!sScreenInitialized) return;

    OSScreenShutdown();

    if (sScreenBufferTV) {
        free(sScreenBufferTV);
        sScreenBufferTV = nullptr;
    }
    if (sScreenBufferDRC) {
        free(sScreenBufferDRC);
        sScreenBufferDRC = nullptr;
    }

    sScreenInitialized = false;
}

/**
 * Draw the menu
 */
static void drawMenu()
{
    // Clear screens (dark blue)
    OSScreenClearBufferEx(SCREEN_TV, 0x000050FF);
    OSScreenClearBufferEx(SCREEN_DRC, 0x000050FF);

    // Header
    OSScreenPutFontEx(SCREEN_TV, 0, 0, "=== Title Switcher ===");
    OSScreenPutFontEx(SCREEN_DRC, 0, 0, "=== Title Switcher ===");

    OSScreenPutFontEx(SCREEN_TV, 0, 1, "UP/DOWN: Navigate | A: Launch | B: Cancel");
    OSScreenPutFontEx(SCREEN_DRC, 0, 1, "UP/DOWN: Navigate | A: Launch | B: Cancel");

    if (sTitleCount == 0) {
        OSScreenPutFontEx(SCREEN_TV, 0, 4, "No other games found!");
        OSScreenPutFontEx(SCREEN_DRC, 0, 4, "No other games found!");
    } else {
        char line[80];
        snprintf(line, sizeof(line), "Games: %d | Selected: %d", sTitleCount, sSelectedIndex + 1);
        OSScreenPutFontEx(SCREEN_TV, 0, 2, line);
        OSScreenPutFontEx(SCREEN_DRC, 0, 2, line);

        // Draw visible titles
        for (int i = 0; i < VISIBLE_TITLES && (sScrollOffset + i) < sTitleCount; i++) {
            int idx = sScrollOffset + i;

            // Truncate name for display
            char displayName[50];
            strncpy(displayName, sTitles[idx].name, sizeof(displayName) - 1);
            displayName[sizeof(displayName) - 1] = '\0';

            if (idx == sSelectedIndex) {
                snprintf(line, sizeof(line), "> %s", displayName);
            } else {
                snprintf(line, sizeof(line), "  %s", displayName);
            }

            OSScreenPutFontEx(SCREEN_TV, 0, 4 + i, line);
            OSScreenPutFontEx(SCREEN_DRC, 0, 4 + i, line);
        }

        // Scroll indicators
        if (sScrollOffset > 0) {
            OSScreenPutFontEx(SCREEN_TV, 55, 4, "[UP]");
            OSScreenPutFontEx(SCREEN_DRC, 45, 4, "[UP]");
        }
        if (sScrollOffset + VISIBLE_TITLES < sTitleCount) {
            OSScreenPutFontEx(SCREEN_TV, 55, 4 + VISIBLE_TITLES - 1, "[DOWN]");
            OSScreenPutFontEx(SCREEN_DRC, 45, 4 + VISIBLE_TITLES - 1, "[DOWN]");
        }
    }

    // Flush and flip
    DCFlushRange(sScreenBufferTV, sScreenSizeTV);
    DCFlushRange(sScreenBufferDRC, sScreenSizeDRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

/**
 * Open menu
 */
static void openMenu()
{
    if (sMenuOpen) return;

    OSReport("[TitleSwitcher] Opening menu...\n");

    if (!loadTitles()) {
        OSReport("[TitleSwitcher] No titles found\n");
        return;
    }

    if (!initScreen()) {
        OSReport("[TitleSwitcher] Screen init failed\n");
        return;
    }

    sSelectedIndex = 0;
    sScrollOffset = 0;
    sMenuOpen = true;

    drawMenu();
}

/**
 * Close menu
 */
static void closeMenu()
{
    if (!sMenuOpen) return;

    OSReport("[TitleSwitcher] Closing menu...\n");
    shutdownScreen();
    sMenuOpen = false;
}

/**
 * Launch selected game
 */
static void launchSelected()
{
    if (sSelectedIndex < 0 || sSelectedIndex >= sTitleCount) return;

    uint64_t titleId = sTitles[sSelectedIndex].titleId;
    OSReport("[TitleSwitcher] Launching %s\n", sTitles[sSelectedIndex].name);

    closeMenu();
    SYSLaunchTitle(titleId);
}

INITIALIZE_PLUGIN()
{
    OSReport("[TitleSwitcher] Plugin initialized\n");
}

DEINITIALIZE_PLUGIN()
{
    if (sMenuOpen) closeMenu();
    OSReport("[TitleSwitcher] Plugin deinitialized\n");
}

/**
 * Common input handler
 */
static void handleInput(VPADStatus *buffer, uint32_t bufferSize)
{
    uint32_t pressed = buffer[0].trigger;

    if (sMenuOpen) {
        if (pressed & VPAD_BUTTON_B) {
            closeMenu();
        }
        else if (pressed & VPAD_BUTTON_A) {
            launchSelected();
        }
        else if (pressed & VPAD_BUTTON_UP) {
            if (sSelectedIndex > 0) {
                sSelectedIndex--;
                if (sSelectedIndex < sScrollOffset) {
                    sScrollOffset = sSelectedIndex;
                }
                drawMenu();
            }
        }
        else if (pressed & VPAD_BUTTON_DOWN) {
            if (sSelectedIndex < sTitleCount - 1) {
                sSelectedIndex++;
                if (sSelectedIndex >= sScrollOffset + VISIBLE_TITLES) {
                    sScrollOffset = sSelectedIndex - VISIBLE_TITLES + 1;
                }
                drawMenu();
            }
        }

        // Block all input while menu is open
        for (uint32_t i = 0; i < bufferSize; i++) {
            buffer[i].trigger = 0;
            buffer[i].hold = 0;
            buffer[i].release = 0;
        }
    }
    else {
        if (pressed & TRIGGER_BUTTON) {
            openMenu();
            for (uint32_t i = 0; i < bufferSize; i++) {
                buffer[i].trigger &= ~TRIGGER_BUTTON;
                buffer[i].hold &= ~TRIGGER_BUTTON;
            }
        }
    }
}

/**
 * VPADRead hook for GAME process
 */
DECL_FUNCTION(int32_t, VPADRead_Game, VPADChan chan, VPADStatus *buffer, uint32_t bufferSize, VPADReadError *error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Game(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleInput(buffer, bufferSize);
    }

    if (error) *error = realError;
    return result;
}

/**
 * VPADRead hook for Wii U Menu
 */
DECL_FUNCTION(int32_t, VPADRead_Menu, VPADChan chan, VPADStatus *buffer, uint32_t bufferSize, VPADReadError *error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Menu(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleInput(buffer, bufferSize);
    }

    if (error) *error = realError;
    return result;
}

// Hook for GAME process
WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead_Game, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_GAME);

// Hook for Wii U Menu
WUPS_MUST_REPLACE_FOR_PROCESS(VPADRead_Menu, WUPS_LOADER_LIBRARY_VPAD, VPADRead, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
