/**
 * Custom Menu Implementation
 *
 * Uses the same rendering pattern as WiiUPluginLoaderBackend's DrawUtils:
 * - Save/restore display controller registers
 * - Use MEMAllocFromMappedMemoryForGX2Ex for framebuffers
 * - Proper initialization sequence
 */

#include "custom_menu.h"
#include "dc.h"
#include "settings.h"

#include <coreinit/screen.h>
#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/title.h>
#include <coreinit/mcp.h>
#include <coreinit/systeminfo.h>
#include <gx2/event.h>
#include <vpad/input.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>
#include <memory/mappedmemory.h>
#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <cctype>

namespace {

constexpr int MAX_TITLES = 512;
constexpr int VISIBLE_ROWS = 15;  // Safe for DRC screen

// Navigation skip amounts
constexpr int SKIP_SMALL = 5;   // Left/Right
constexpr int SKIP_LARGE = 15;   // L/R triggers

// Cached title data (loaded once at startup)
bool sTitlesLoaded = false;

struct TitleEntry {
    uint64_t titleId;
    char name[64];
};

// Menu state
bool sIsOpen = false;
bool sInitialized = false;

// Title data
TitleEntry sTitles[MAX_TITLES];
int sTitleCount = 0;

// Screen buffers
void* sBufferTV = nullptr;
void* sBufferDRC = nullptr;
uint32_t sBufferSizeTV = 0;
uint32_t sBufferSizeDRC = 0;

// Selection state
int sSelectedIndex = 0;
int sScrollOffset = 0;

/**
 * Get title name from system metadata
 */
void getTitleName(uint64_t titleId, char* outName, size_t maxLen)
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
            snprintf(outName, maxLen, "%.63s", name);
        } else {
            snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
        }
    } else {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
    }

    free(metaXml);
}

/**
 * Load all game titles from system (cached - only loads once)
 */
void loadTitles(bool forceReload = false)
{
    // Use cached data if available
    if (sTitlesLoaded && !forceReload) {
        return;
    }

    sTitleCount = 0;

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

        // Sort alphabetically
        for (int i = 0; i < sTitleCount - 1; i++) {
            for (int j = 0; j < sTitleCount - i - 1; j++) {
                if (strcasecmp(sTitles[j].name, sTitles[j + 1].name) > 0) {
                    TitleEntry temp = sTitles[j];
                    sTitles[j] = sTitles[j + 1];
                    sTitles[j + 1] = temp;
                }
            }
        }
    }

    free(titleList);
    MCP_Close(mcpHandle);
    sTitlesLoaded = true;
}

/**
 * Draw text on both screens at given row
 */
void drawText(int col, int row, const char* text)
{
    OSScreenPutFontEx(SCREEN_TV, col, row, text);
    OSScreenPutFontEx(SCREEN_DRC, col, row, text);
}

/**
 * Render the menu to both screens
 */
void renderMenu()
{
    // Clear both buffers with background color from settings
    OSScreenClearBufferEx(SCREEN_TV, Settings::gSettings.bgColor);
    OSScreenClearBufferEx(SCREEN_DRC, Settings::gSettings.bgColor);

    // Compact header
    char headerLine[80];
    snprintf(headerLine, sizeof(headerLine),
             "TITLE SWITCHER [%d/%d] A:Go B:Close Y:Fav +:Settings",
             sSelectedIndex + 1, sTitleCount);
    drawText(0, 0, headerLine);
    drawText(0, 1, "------------------------------------------------");

    if (sTitleCount == 0) {
        drawText(2, 5, "No games found!");
    } else {
        // Draw visible titles starting at row 2
        for (int i = 0; i < VISIBLE_ROWS && (sScrollOffset + i) < sTitleCount; i++) {
            int idx = sScrollOffset + i;
            char line[80];

            // Check if this title is a favorite
            bool isFav = Settings::IsFavorite(sTitles[idx].titleId);

            // Truncate name to fit (leave room for number prefix and star)
            char displayName[46];
            strncpy(displayName, sTitles[idx].name, sizeof(displayName) - 1);
            displayName[sizeof(displayName) - 1] = '\0';

            // Format: "> 001. * Game Name" or "  001.   Game Name"
            const char* favMark = isFav ? "*" : " ";

            if (idx == sSelectedIndex) {
                snprintf(line, sizeof(line), "> %3d. %s %-44s", idx + 1, favMark, displayName);
            } else {
                snprintf(line, sizeof(line), "  %3d. %s %-44s", idx + 1, favMark, displayName);
            }

            drawText(0, 2 + i, line);
        }

        // Scroll indicators on the right
        if (sScrollOffset > 0) {
            drawText(58, 2, "[UP]");
        }
        if (sScrollOffset + VISIBLE_ROWS < sTitleCount) {
            drawText(56, 2 + VISIBLE_ROWS - 1, "[DOWN]");
        }
    }

    // Flush caches and flip buffers
    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

/**
 * Render a simple settings submenu
 */
void renderSettingsMenu(int settingIndex)
{
    OSScreenClearBufferEx(SCREEN_TV, Settings::gSettings.bgColor);
    OSScreenClearBufferEx(SCREEN_DRC, Settings::gSettings.bgColor);

    drawText(0, 0, "SETTINGS                           B:Back");
    drawText(0, 1, "------------------------------------------------");

    const char* options[] = {
        "Background Color",
        "Reset to Defaults",
        "Save Settings",
    };
    const int numOptions = 3;

    for (int i = 0; i < numOptions; i++) {
        char line[64];
        if (i == settingIndex) {
            snprintf(line, sizeof(line), "> %s", options[i]);
        } else {
            snprintf(line, sizeof(line), "  %s", options[i]);
        }
        drawText(0, 3 + i, line);
    }

    // Show current values
    char info[64];
    snprintf(info, sizeof(info), "BG Color: 0x%08X", Settings::gSettings.bgColor);
    drawText(0, 10, info);
    snprintf(info, sizeof(info), "Favorites: %d", (int)Settings::gSettings.favorites.size());
    drawText(0, 11, info);

    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

/**
 * Run the settings submenu
 */
void runSettingsMenu()
{
    int settingIndex = 0;
    const int numOptions = 3;
    VPADStatus vpadStatus;
    VPADReadError vpadError;
    bool inSettings = true;

    while (inSettings) {
        GX2WaitForVsync();
        renderSettingsMenu(settingIndex);

        int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);
        if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
            uint32_t pressed = vpadStatus.trigger;

            if (pressed & VPAD_BUTTON_B) {
                inSettings = false;
            }

            if (pressed & VPAD_BUTTON_UP) {
                if (settingIndex > 0) settingIndex--;
            }

            if (pressed & VPAD_BUTTON_DOWN) {
                if (settingIndex < numOptions - 1) settingIndex++;
            }

            if (pressed & VPAD_BUTTON_A) {
                switch (settingIndex) {
                    case 0: // Cycle background color
                        {
                            // Cycle through some preset colors
                            uint32_t colors[] = {
                                0x1E1E2EFF,  // Catppuccin Base
                                0x11111BFF,  // Catppuccin Crust
                                0x181825FF,  // Catppuccin Mantle
                                0x000000FF,  // Black
                                0x1A1A2EFF,  // Dark blue
                                0x2D2D44FF,  // Slate
                            };
                            const int numColors = 6;
                            for (int i = 0; i < numColors; i++) {
                                if (Settings::gSettings.bgColor == colors[i]) {
                                    Settings::gSettings.bgColor = colors[(i + 1) % numColors];
                                    break;
                                }
                                if (i == numColors - 1) {
                                    Settings::gSettings.bgColor = colors[0];
                                }
                            }
                        }
                        break;
                    case 1: // Reset to defaults
                        Settings::gSettings = Settings::PluginSettings();
                        break;
                    case 2: // Save settings
                        Settings::Save();
                        break;
                }
            }
        }
    }
}

/**
 * Main menu loop - runs until user exits or selects a game
 * Returns titleId to launch, or 0 if cancelled
 */
uint64_t runMenuLoop()
{
    uint64_t selectedTitle = 0;
    VPADStatus vpadStatus;
    VPADReadError vpadError;

    while (sIsOpen) {
        // Wait for vsync
        GX2WaitForVsync();

        // Render current state
        renderMenu();

        // Read input
        int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

        if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
            uint32_t pressed = vpadStatus.trigger;

            // B = Close menu
            if (pressed & VPAD_BUTTON_B) {
                sIsOpen = false;
                break;
            }

            // A = Select and launch
            if (pressed & VPAD_BUTTON_A) {
                if (sTitleCount > 0 && sSelectedIndex >= 0 && sSelectedIndex < sTitleCount) {
                    selectedTitle = sTitles[sSelectedIndex].titleId;
                    sIsOpen = false;
                    break;
                }
            }

            // Up = Move selection up
            if (pressed & VPAD_BUTTON_UP) {
                if (sSelectedIndex > 0) {
                    sSelectedIndex--;
                    if (sSelectedIndex < sScrollOffset) {
                        sScrollOffset = sSelectedIndex;
                    }
                }
            }

            // Down = Move selection down
            if (pressed & VPAD_BUTTON_DOWN) {
                if (sSelectedIndex < sTitleCount - 1) {
                    sSelectedIndex++;
                    if (sSelectedIndex >= sScrollOffset + VISIBLE_ROWS) {
                        sScrollOffset = sSelectedIndex - VISIBLE_ROWS + 1;
                    }
                }
            }

            // Left = Skip 10 up
            if (pressed & VPAD_BUTTON_LEFT) {
                sSelectedIndex -= SKIP_SMALL;
                if (sSelectedIndex < 0) sSelectedIndex = 0;
                if (sSelectedIndex < sScrollOffset) {
                    sScrollOffset = sSelectedIndex;
                }
            }

            // Right = Skip 10 down
            if (pressed & VPAD_BUTTON_RIGHT) {
                sSelectedIndex += SKIP_SMALL;
                if (sSelectedIndex >= sTitleCount) sSelectedIndex = sTitleCount - 1;
                if (sSelectedIndex >= sScrollOffset + VISIBLE_ROWS) {
                    sScrollOffset = sSelectedIndex - VISIBLE_ROWS + 1;
                }
            }

            // L trigger = Skip 50 up
            if (pressed & VPAD_BUTTON_L) {
                sSelectedIndex -= SKIP_LARGE;
                if (sSelectedIndex < 0) sSelectedIndex = 0;
                if (sSelectedIndex < sScrollOffset) {
                    sScrollOffset = sSelectedIndex;
                }
            }

            // R trigger = Skip 50 down
            if (pressed & VPAD_BUTTON_R) {
                sSelectedIndex += SKIP_LARGE;
                if (sSelectedIndex >= sTitleCount) sSelectedIndex = sTitleCount - 1;
                if (sSelectedIndex >= sScrollOffset + VISIBLE_ROWS) {
                    sScrollOffset = sSelectedIndex - VISIBLE_ROWS + 1;
                }
            }

            // Y = Toggle favorite for current selection
            if (pressed & VPAD_BUTTON_Y) {
                if (sTitleCount > 0 && sSelectedIndex >= 0 && sSelectedIndex < sTitleCount) {
                    Settings::ToggleFavorite(sTitles[sSelectedIndex].titleId);
                }
            }

            // + = Open settings menu
            if (pressed & VPAD_BUTTON_PLUS) {
                runSettingsMenu();
            }
        }
    }

    return selectedTitle;
}

} // anonymous namespace

namespace CustomMenu {

void Init()
{
    sInitialized = true;
}

void PreloadTitles()
{
    // Load titles at startup so menu opens instantly
    loadTitles();
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

void Open()
{
    if (sIsOpen) return;

    // Load titles
    loadTitles();

    // Save current home button state
    bool wasHomeButtonMenuEnabled = OSIsHomeButtonMenuEnabled();

    // Save display controller registers
    auto tvRender1 = DCReadReg32(SCREEN_TV, D1GRPH_CONTROL_REG);
    auto tvRender2 = DCReadReg32(SCREEN_TV, D1GRPH_ENABLE_REG);
    auto tvPitch1  = DCReadReg32(SCREEN_TV, D1GRPH_PITCH_REG);
    auto tvPitch2  = DCReadReg32(SCREEN_TV, D1OVL_PITCH_REG);

    auto drcRender1 = DCReadReg32(SCREEN_DRC, D1GRPH_CONTROL_REG);
    auto drcRender2 = DCReadReg32(SCREEN_DRC, D1GRPH_ENABLE_REG);
    auto drcPitch1  = DCReadReg32(SCREEN_DRC, D1GRPH_PITCH_REG);
    auto drcPitch2  = DCReadReg32(SCREEN_DRC, D1OVL_PITCH_REG);

    // Initialize OSScreen
    OSScreenInit();

    // Get buffer sizes
    sBufferSizeTV = OSScreenGetBufferSizeEx(SCREEN_TV);
    sBufferSizeDRC = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // Allocate buffers using mapped memory (required for GX2 compatibility)
    sBufferTV = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeTV, 0x100);
    sBufferDRC = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeDRC, 0x100);

    if (!sBufferTV || !sBufferDRC) {
        // Allocation failed - cleanup and abort
        if (sBufferTV) MEMFreeToMappedMemory(sBufferTV);
        if (sBufferDRC) MEMFreeToMappedMemory(sBufferDRC);
        sBufferTV = nullptr;
        sBufferDRC = nullptr;

        // Restore DC registers
        DCWriteReg32(SCREEN_TV, D1GRPH_CONTROL_REG, tvRender1);
        DCWriteReg32(SCREEN_TV, D1GRPH_ENABLE_REG, tvRender2);
        DCWriteReg32(SCREEN_TV, D1GRPH_PITCH_REG, tvPitch1);
        DCWriteReg32(SCREEN_TV, D1OVL_PITCH_REG, tvPitch2);
        DCWriteReg32(SCREEN_DRC, D1GRPH_CONTROL_REG, drcRender1);
        DCWriteReg32(SCREEN_DRC, D1GRPH_ENABLE_REG, drcRender2);
        DCWriteReg32(SCREEN_DRC, D1GRPH_PITCH_REG, drcPitch1);
        DCWriteReg32(SCREEN_DRC, D1OVL_PITCH_REG, drcPitch2);
        return;
    }

    // Set up screen buffers
    OSScreenSetBufferEx(SCREEN_TV, sBufferTV);
    OSScreenSetBufferEx(SCREEN_DRC, sBufferDRC);

    // Initialize both buffers (clear + flip twice to ensure both front/back are ready)
    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);
    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    OSScreenClearBufferEx(SCREEN_TV, 0);
    OSScreenClearBufferEx(SCREEN_DRC, 0);
    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);

    // Enable screens
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    // Disable home button menu while our menu is open
    OSEnableHomeButtonMenu(false);

    // Restore last selection from settings (clamped to valid range)
    sSelectedIndex = Settings::gSettings.lastIndex;
    if (sSelectedIndex >= sTitleCount) sSelectedIndex = sTitleCount > 0 ? sTitleCount - 1 : 0;
    if (sSelectedIndex < 0) sSelectedIndex = 0;

    // Adjust scroll offset to show selected item
    sScrollOffset = sSelectedIndex - (VISIBLE_ROWS / 2);
    if (sScrollOffset < 0) sScrollOffset = 0;
    if (sScrollOffset + VISIBLE_ROWS > sTitleCount) {
        sScrollOffset = sTitleCount - VISIBLE_ROWS;
        if (sScrollOffset < 0) sScrollOffset = 0;
    }

    sIsOpen = true;

    // Run the menu loop
    uint64_t titleToLaunch = runMenuLoop();

    // Save last selected index
    Settings::gSettings.lastIndex = sSelectedIndex;
    Settings::Save();

    // Cleanup
    sIsOpen = false;

    // Re-enable home button menu
    OSEnableHomeButtonMenu(wasHomeButtonMenuEnabled);

    // Restore display controller registers
    DCWriteReg32(SCREEN_TV, D1GRPH_CONTROL_REG, tvRender1);
    DCWriteReg32(SCREEN_TV, D1GRPH_ENABLE_REG, tvRender2);
    DCWriteReg32(SCREEN_TV, D1GRPH_PITCH_REG, tvPitch1);
    DCWriteReg32(SCREEN_TV, D1OVL_PITCH_REG, tvPitch2);
    DCWriteReg32(SCREEN_DRC, D1GRPH_CONTROL_REG, drcRender1);
    DCWriteReg32(SCREEN_DRC, D1GRPH_ENABLE_REG, drcRender2);
    DCWriteReg32(SCREEN_DRC, D1GRPH_PITCH_REG, drcPitch1);
    DCWriteReg32(SCREEN_DRC, D1OVL_PITCH_REG, drcPitch2);

    // Free buffers
    if (sBufferTV) {
        MEMFreeToMappedMemory(sBufferTV);
        sBufferTV = nullptr;
    }
    if (sBufferDRC) {
        MEMFreeToMappedMemory(sBufferDRC);
        sBufferDRC = nullptr;
    }

    // Launch selected title if any
    if (titleToLaunch != 0) {
        SYSLaunchTitle(titleToLaunch);
    }
}

void Close()
{
    sIsOpen = false;
}

bool Update(uint32_t buttonsTriggered, uint32_t buttonsHeld)
{
    // Not used in blocking approach
    return sIsOpen;
}

} // namespace CustomMenu
