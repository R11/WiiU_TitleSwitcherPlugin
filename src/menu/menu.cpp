/**
 * Main Menu Implementation
 *
 * See menu.h for usage documentation.
 */

#include "menu.h"
#include "categories.h"
#include "../render/renderer.h"
#include "../render/image_loader.h"
#include "../input/buttons.h"
#include "../input/text_input.h"
#include "../titles/titles.h"
#include "../storage/settings.h"
#include "../presets/title_presets.h"
#include "../ui/list_view.h"

// Wii U SDK headers
#include <vpad/input.h>           // VPADRead, VPADStatus
#include <sysapp/launch.h>        // SYSLaunchTitle, SYSLaunchMenu, _SYSLaunchSettings
#include <sysapp/switch.h>        // SYSSwitchToBrowser, SYSSwitchToEShop, etc.
#include <sysapp/title.h>         // SYSTEM_APP_ID_*
#include <nn/ccr/sys.h>           // CCRSysGetCurrentLCDMode, CCRSysSetCurrentLCDMode
#include <coreinit/time.h>        // OSGetTime, OSTicksToMilliseconds
#include <proc_ui/procui.h>       // ProcUIIsRunning

// Standard library
#include <cstdio>                 // snprintf
#include <cstring>                // strlen, strncpy

namespace Menu {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Current menu state
bool sIsOpen = false;
bool sInitialized = false;
Mode sCurrentMode = Mode::BROWSE;

// Selection state
UI::ListView::State sTitleListState;  // Title list state (browse mode)

// Edit mode state
UI::ListView::State sEditCatsListState;  // Category edit list state

// Settings mode state
enum class SettingsSubMode {
    MAIN,           // Main settings list
    MANAGE_CATS,    // Category management submenu
    SYSTEM_APPS,    // System apps submenu
    COLOR_INPUT,    // Editing a color value
    NAME_INPUT      // Editing a category name
};

// =============================================================================
// Data-Driven Settings System
// =============================================================================

enum class SettingType {
    TOGGLE,     // Boolean on/off
    COLOR,      // RGBA hex color
    BRIGHTNESS, // Gamepad brightness (1-5)
    ACTION      // Opens a submenu or performs an action
};

enum SettingAction {
    ACTION_MANAGE_CATEGORIES = -1,
    ACTION_SYSTEM_APPS = -2,
    ACTION_DEBUG_GRID = -3
};

// =============================================================================
// System Apps Data
// =============================================================================

// System app options for the System Apps submenu
struct SystemAppOption {
    const char* name;
    const char* description;
    int appId;  // SYSTEM_APP_ID_* or special negative values
};

// Special app IDs for actions that use different APIs
constexpr int SYSAPP_RETURN_TO_MENU = -1;
constexpr int SYSAPP_BROWSER = -2;
constexpr int SYSAPP_ESHOP = -3;
constexpr int SYSAPP_CONTROLLER_SYNC = -4;

static const SystemAppOption sSystemApps[] = {
    {"Return to Menu",       "Exit game and return to Wii U Menu",         SYSAPP_RETURN_TO_MENU},
    {"Internet Browser",     "Open the Internet Browser",                   SYSAPP_BROWSER},
    {"Nintendo eShop",       "Open the Nintendo eShop",                     SYSAPP_ESHOP},
    {"Mii Maker",            "Open Mii Maker",                              SYSTEM_APP_ID_MII_MAKER},
    {"System Settings",      "Open System Settings",                        SYSTEM_APP_ID_SYSTEM_SETTINGS},
    {"Controller Sync",      "Sync controllers (Gamepad, Wiimotes)",        SYSAPP_CONTROLLER_SYNC},
    {"Notifications",        "View system notifications",                   SYSTEM_APP_ID_NOTIFICATIONS},
    {"User Settings",        "Manage user accounts",                        SYSTEM_APP_ID_USER_SETTINGS},
    {"Parental Controls",    "Open Parental Controls",                      SYSTEM_APP_ID_PARENTAL_CONTROLS},
    {"Daily Log",            "View play activity",                          SYSTEM_APP_ID_DAILY_LOG},
};

static constexpr int SYSTEM_APP_COUNT = sizeof(sSystemApps) / sizeof(sSystemApps[0]);

// ListView state for system apps submenu
UI::ListView::State sSystemAppsListState;

struct SettingItem {
    const char* name;
    const char* descLine1;
    const char* descLine2;
    SettingType type;
    int dataOffset;  // offsetof() for TOGGLE/COLOR, action ID for ACTION
};

// Helper macros for cleaner definitions
#define TOGGLE_SETTING(n, d1, d2, member) \
    {n, d1, d2, SettingType::TOGGLE, (int)offsetof(Settings::PluginSettings, member)}
#define COLOR_SETTING(n, d1, d2, member) \
    {n, d1, d2, SettingType::COLOR, (int)offsetof(Settings::PluginSettings, member)}
#define BRIGHTNESS_SETTING(n, d1, d2) \
    {n, d1, d2, SettingType::BRIGHTNESS, 0}
#define ACTION_SETTING(n, d1, d2, actionId) \
    {n, d1, d2, SettingType::ACTION, actionId}

// All settings defined in one place - easy to add/remove/reorder
static const SettingItem sSettingItems[] = {
    BRIGHTNESS_SETTING("Gamepad Brightness", "Adjust Gamepad screen",   "brightness (1-5)."),
    ACTION_SETTING("System Apps",       "Launch system applications",   "(Browser, Settings, etc.)", ACTION_SYSTEM_APPS),
    TOGGLE_SETTING("Show Numbers",      "Show line numbers before",     "each title in the list.",   showNumbers),
    TOGGLE_SETTING("Show Favorites",    "Show favorite marker (*)",     "in the title list.",        showFavorites),
    COLOR_SETTING("Background",         "Menu background color.",       "RGBA hex format.",          bgColor),
    COLOR_SETTING("Title Text",         "Normal title text color.",     "RGBA hex format.",          titleColor),
    COLOR_SETTING("Highlighted",        "Selected title color.",        "RGBA hex format.",          highlightedTitleColor),
    COLOR_SETTING("Favorite",           "Favorite marker color.",       "RGBA hex format.",          favoriteColor),
    COLOR_SETTING("Header",             "Header text color.",           "RGBA hex format.",          headerColor),
    COLOR_SETTING("Category",           "Category tab color.",          "RGBA hex format.",          categoryColor),
    ACTION_SETTING("Manage Categories", "Create, rename, or delete",    "custom categories.",        ACTION_MANAGE_CATEGORIES),
    ACTION_SETTING("Debug Grid",        "Show grid overlay with",       "dimensions and positions.", ACTION_DEBUG_GRID),
};

static constexpr int SETTINGS_ITEM_COUNT = sizeof(sSettingItems) / sizeof(sSettingItems[0]);

UI::ListView::State sSettingsListState;  // Settings list state
SettingsSubMode sSettingsSubMode = SettingsSubMode::MAIN;
UI::ListView::State sManageCatsListState;  // Manage categories list state
int sEditingSettingIndex = -1;       // Which setting is being edited (-1 = none)
int sEditingCategoryId = -1;         // Which category is being renamed (-1 = new)
TextInput::Field sInputField;        // Reusable text input field

// Helper to get a toggle value pointer from offset
bool* getTogglePtr(int offset) {
    return (bool*)((char*)&Settings::Get() + offset);
}

// Helper to get a color value pointer from offset
uint32_t* getColorPtr(int offset) {
    return (uint32_t*)((char*)&Settings::Get() + offset);
}

// Safety tracking
OSTime sApplicationStartTime = 0;       // When current app started
bool sInForeground = false;             // Is app in foreground?
constexpr uint32_t STARTUP_GRACE_MS = 3000;  // Wait time before allowing menu

// =============================================================================
// Layout Constants
// =============================================================================

// Visible rows in different scrollable lists
constexpr int CATEGORY_EDIT_VISIBLE_ROWS = 10;    // Category checkboxes in edit mode
constexpr int CATEGORY_MANAGE_VISIBLE_ROWS = 10;  // Category list in manage mode (minus header rows)

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Clamp selection to valid range and update scroll offset.
 */
void clampSelection()
{
    int count = Categories::GetFilteredCount();
    sTitleListState.SetItemCount(count, Renderer::GetVisibleRows());
}

/**
 * Draw the category bar at the top of the screen.
 */
void drawCategoryBar()
{
    char line[80];
    int col = 0;

    // Draw each visible category tab
    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    for (int i = 0; i < catCount && col < 60; i++) {
        // Skip hidden categories
        if (!Categories::IsCategoryVisible(i)) {
            continue;
        }

        const char* name = Categories::GetCategoryName(i);

        if (i == currentCat) {
            // Highlighted category
            snprintf(line, sizeof(line), "[%s] ", name);
        } else {
            snprintf(line, sizeof(line), " %s  ", name);
        }

        Renderer::DrawText(col, CATEGORY_ROW, line);
        col += strlen(line);
    }
}

/**
 * Draw a vertical divider line between the title list and details panel.
 */
void drawDivider()
{
    // Draw vertical bar from list start to footer (not in header)
    for (int row = LIST_START_ROW; row < Renderer::GetFooterRow(); row++) {
        Renderer::DrawText(Renderer::GetDividerCol(), row, "|");
    }
}

/**
 * Draw the title list on the left side.
 */
void drawTitleList()
{
    int count = Categories::GetFilteredCount();
    sTitleListState.itemCount = count;

    // Configure list view
    UI::ListView::Config listConfig;
    listConfig.col = LIST_START_COL;
    listConfig.row = LIST_START_ROW;
    listConfig.width = Renderer::GetListWidth();
    listConfig.visibleRows = Renderer::GetVisibleRows();
    listConfig.showLineNumbers = Settings::Get().showNumbers;
    listConfig.showScrollIndicators = true;

    // Render using ListView
    UI::ListView::Render(sTitleListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(index);

        if (!title) {
            view.text = "(error)";
            return view;
        }

        view.text = title->name;
        view.prefix = isSelected ? "> " : "  ";

        // Favorite marker as suffix if enabled
        if (Settings::Get().showFavorites && Settings::IsFavorite(title->titleId)) {
            view.suffix = " *";
        }

        return view;
    });
}

/**
 * Draw the details panel on the right side.
 */
void drawDetailsPanel()
{
    int count = Categories::GetFilteredCount();
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    if (count == 0 || selectedIdx < 0 || selectedIdx >= count) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
    if (!title) return;

    // Request icon loading (will be cached if already loaded)
    ImageLoader::Request(title->titleId, ImageLoader::Priority::HIGH);

    // Title name (left-aligned with details panel)
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, title->name);

    // Draw icon below title
    // Position calculated from screen percentages for accuracy
    constexpr int ICON_SIZE = 128;
    constexpr int ICON_MARGIN = 170;  // Extra margin from panel edge
    int screenWidth = Renderer::GetScreenWidth();
    int screenHeight = Renderer::GetScreenHeight();
    int gridWidth = Renderer::GetGridWidth();
    int gridHeight = Renderer::GetGridHeight();
    int iconX = (screenWidth * Renderer::GetDetailsPanelCol()) / gridWidth + ICON_MARGIN;
    // Y position: row 4 (after title at row 2 and some spacing)
    int iconY = (screenHeight * 4) / gridHeight;

    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, ICON_SIZE, ICON_SIZE);
    } else {
        // Draw placeholder while loading
        Renderer::DrawPlaceholder(iconX, iconY, ICON_SIZE, ICON_SIZE, 0x333333FF);
    }

    // Info starts after icon (icon is ~5-6 rows at 24px/row)
    constexpr int INFO_START_ROW = LIST_START_ROW + 7;
    int currentRow = INFO_START_ROW;

    // Title ID
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "ID: %016llX",
             static_cast<unsigned long long>(title->titleId));
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), currentRow++, idStr);

    // Favorite status
    const char* favStatus = Settings::IsFavorite(title->titleId) ? "Yes" : "No";
    Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Favorite: %s", favStatus);

    // Game ID (product code) for debugging preset matching
    if (title->productCode[0] != '\0') {
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Game ID: %s", title->productCode);
    } else {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), currentRow++, "Game ID: (none)");
    }

    // Look up preset metadata if available
    const TitlePresets::TitlePreset* preset = nullptr;
    if (title->productCode[0] != '\0') {
        preset = TitlePresets::GetPresetByGameId(title->productCode);
    }

    // Display preset metadata if found
    if (preset) {
        currentRow++;  // Blank line before preset info

        if (preset->publisher[0] != '\0') {
            Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Pub: %s", preset->publisher);
        }

        if (preset->developer[0] != '\0' && currentRow < Renderer::GetFooterRow() - 3) {
            Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Dev: %s", preset->developer);
        }

        // Release date
        if (preset->releaseYear > 0 && currentRow < Renderer::GetFooterRow() - 3) {
            if (preset->releaseMonth > 0 && preset->releaseDay > 0) {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Released: %04d-%02d-%02d",
                                   preset->releaseYear, preset->releaseMonth, preset->releaseDay);
            } else if (preset->releaseMonth > 0) {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Released: %04d-%02d",
                                   preset->releaseYear, preset->releaseMonth);
            } else {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Released: %04d",
                                   preset->releaseYear);
            }
        }

        // Genre and region combined if space allows
        if (currentRow < Renderer::GetFooterRow() - 3) {
            if (preset->genre[0] != '\0' && preset->region[0] != '\0') {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "%s / %s",
                                   preset->genre, preset->region);
            } else if (preset->genre[0] != '\0') {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Genre: %s", preset->genre);
            } else if (preset->region[0] != '\0') {
                Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), currentRow++, "Region: %s", preset->region);
            }
        }
    }

    // Categories this title belongs to (only if there's room)
    if (currentRow < Renderer::GetFooterRow() - 2) {
        currentRow++;  // Blank line before categories
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), currentRow++, "Categories:");

        uint16_t catIds[Settings::MAX_CATEGORIES];
        int catCount = Settings::GetCategoriesForTitle(title->titleId, catIds, Settings::MAX_CATEGORIES);

        if (catCount == 0) {
            Renderer::DrawText(Renderer::GetDetailsPanelCol() + 2, currentRow, "(none)");
        } else {
            for (int i = 0; i < catCount && currentRow < Renderer::GetFooterRow() - 1; i++) {
                const Settings::Category* cat = Settings::GetCategory(catIds[i]);
                if (cat) {
                    Renderer::DrawTextF(Renderer::GetDetailsPanelCol() + 2, currentRow++, "- %s", cat->name);
                }
            }
        }
    }
}

/**
 * Draw the footer with controls and status.
 */
void drawFooter()
{
    int count = Categories::GetFilteredCount();
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);

    // Bottom row: main controls with customizable button labels
    char footer[100];
    snprintf(footer, sizeof(footer),
             "%s:Go %s:Close %s:Fav %s:Edit %s:Settings ZL/ZR:Cat [%d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             Buttons::Actions::FAVORITE.label,
             Buttons::Actions::EDIT.label,
             Buttons::Actions::SETTINGS.label,
             selectedIdx + 1,
             count);

    Renderer::DrawText(0, Renderer::GetFooterRow(), footer);
}

/**
 * Render the main browse mode screen.
 */
void renderBrowseMode()
{
    drawCategoryBar();
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");
    drawDivider();
    drawTitleList();
    drawDetailsPanel();
    drawFooter();
}

/**
 * Handle input in browse mode.
 * @param pressed Buttons just pressed this frame
 * @return Title ID to launch, or 0 if none
 */
uint64_t handleBrowseModeInput(uint32_t pressed)
{
    int count = Categories::GetFilteredCount();
    sTitleListState.itemCount = count;

    // Configure ListView for navigation
    UI::ListView::Config listConfig;
    listConfig.visibleRows = Renderer::GetVisibleRows();
    listConfig.smallSkip = Buttons::Skip::SMALL;
    listConfig.largeSkip = Buttons::Skip::LARGE;
    listConfig.canFavorite = true;  // Y for favorite toggle

    // Handle navigation via ListView
    UI::ListView::HandleInput(sTitleListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sTitleListState);

    // Category navigation (ZL/ZR) - handled separately as these change the list
    if (Buttons::Actions::CATEGORY_PREV.Pressed(pressed)) {
        Categories::PreviousCategory();
        sTitleListState = UI::ListView::State();  // Reset list state
        clampSelection();
    }
    if (Buttons::Actions::CATEGORY_NEXT.Pressed(pressed)) {
        Categories::NextCategory();
        sTitleListState = UI::ListView::State();  // Reset list state
        clampSelection();
    }

    // Handle actions
    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::FAVORITE:
            // Toggle favorite
            if (selectedIdx >= 0 && selectedIdx < count) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
                if (title) {
                    Settings::ToggleFavorite(title->titleId);
                    // Re-filter in case we're in Favorites category
                    Categories::RefreshFilter();
                    clampSelection();
                }
            }
            break;

        case UI::ListView::Action::CANCEL:
            // Close menu
            sIsOpen = false;
            return 0;

        case UI::ListView::Action::CONFIRM:
            // Launch title
            if (selectedIdx >= 0 && selectedIdx < count) {
                const Titles::TitleInfo* title = Categories::GetFilteredTitle(selectedIdx);
                if (title) {
                    sIsOpen = false;
                    return title->titleId;
                }
            }
            break;

        default:
            break;
    }

    // Edit mode (X) - handled separately as it's not a standard ListView action
    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        sEditCatsListState = UI::ListView::State();  // Reset list state
        sCurrentMode = Mode::EDIT;
    }

    // Settings (Plus) - handled separately
    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        sSettingsListState = UI::ListView::State();  // Reset list state
        sSettingsSubMode = SettingsSubMode::MAIN;
        sCurrentMode = Mode::SETTINGS;
    }

    return 0;
}

/**
 * Render the edit mode screen.
 * Left side: Title list (read-only)
 * Right side: Category checkboxes
 */
void renderEditMode()
{
    // Get the title being edited
    int titleIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(titleIdx);
    if (!title) {
        Renderer::DrawText(0, 0, "Error: No title selected");
        return;
    }

    // --- Header ---
    Renderer::DrawText(0, 0, "EDIT TITLE CATEGORIES");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    // --- Left side: Title info (abbreviated) ---
    char nameLine[32];
    strncpy(nameLine, title->name, 28);
    nameLine[28] = '\0';
    Renderer::DrawTextF(0, LIST_START_ROW, "> %s", nameLine);

    // Show title ID
    Renderer::DrawTextF(0, LIST_START_ROW + 2, "ID: %016llX",
                      static_cast<unsigned long long>(title->titleId));

    // --- Divider ---
    drawDivider();

    // --- Right side: Category checkboxes using ListView ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Categories:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "------------");

    // Get user-defined categories
    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;
    uint64_t titleId = title->titleId;

    if (catCount == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3,
                         "No categories defined.");
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4,
                         "Create in Settings (+)");
    } else {
        UI::ListView::Config listConfig;
        listConfig.col = Renderer::GetDetailsPanelCol();
        listConfig.row = LIST_START_ROW + 3;
        listConfig.width = Renderer::GetGridWidth() - Renderer::GetDetailsPanelCol() - 1;
        listConfig.visibleRows = CATEGORY_EDIT_VISIBLE_ROWS;
        listConfig.showScrollIndicators = true;
        listConfig.canToggle = true;

        sEditCatsListState.itemCount = catCount;

        UI::ListView::Render(sEditCatsListState, listConfig, [&categories, titleId](int index, bool isSelected) {
            UI::ListView::ItemView view;
            const Settings::Category& cat = categories[index];
            bool inCategory = Settings::TitleHasCategory(titleId, cat.id);

            // Build prefix with cursor and checkbox
            static char prefixBuf[16];
            snprintf(prefixBuf, sizeof(prefixBuf), "%s%s ",
                    isSelected ? ">" : " ",
                    inCategory ? "[X]" : "[ ]");
            view.prefix = prefixBuf;
            view.text = cat.name;
            return view;
        });
    }

    // --- Footer ---
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

/**
 * Handle input in edit mode.
 * @param pressed Buttons just pressed this frame
 */
void handleEditModeInput(uint32_t pressed)
{
    int titleIdx = UI::ListView::GetSelectedIndex(sTitleListState);
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(titleIdx);
    if (!title) {
        // No title selected - go back
        sCurrentMode = Mode::BROWSE;
        return;
    }

    int catCount = Settings::GetCategoryCount();

    // Configure ListView for checkbox-style list
    UI::ListView::Config listConfig;
    listConfig.visibleRows = CATEGORY_EDIT_VISIBLE_ROWS;
    listConfig.canToggle = true;
    listConfig.canCancel = true;

    sEditCatsListState.itemCount = catCount;

    // Handle navigation
    UI::ListView::HandleInput(sEditCatsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sEditCatsListState);

    // Handle actions
    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::TOGGLE:
            // Toggle category assignment
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount) {
                const auto& categories = Settings::Get().categories;
                uint16_t catId = categories[selectedIdx].id;

                if (Settings::TitleHasCategory(title->titleId, catId)) {
                    Settings::RemoveTitleFromCategory(title->titleId, catId);
                } else {
                    Settings::AssignTitleToCategory(title->titleId, catId);
                }
                // Refresh the filter in case this affects the current view
                Categories::RefreshFilter();
            }
            break;

        case UI::ListView::Action::CANCEL:
            // Save changes before exiting
            Settings::Save();
            sCurrentMode = Mode::BROWSE;
            break;

        default:
            break;
    }
}

/**
 * Get current gamepad brightness level (1-5).
 */
int getCurrentBrightness()
{
    CCRSysLCDMode mode;
    if (CCRSysGetCurrentLCDMode(&mode) == 0) {
        return static_cast<int>(mode) + 1;  // Convert 0-4 to 1-5
    }
    return 3;  // Default to middle if read fails
}

/**
 * Set gamepad brightness level (1-5).
 */
void setBrightness(int level)
{
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    CCRSysLCDMode mode = static_cast<CCRSysLCDMode>(level - 1);  // Convert 1-5 to 0-4
    CCRSysSetCurrentLCDMode(mode);
}

/**
 * Render the main settings list.
 */
void renderSettingsMain()
{
    // --- Header ---
    Renderer::DrawText(0, 0, "SETTINGS");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    // --- Divider ---
    drawDivider();

    // --- Left side: Settings list using ListView ---
    UI::ListView::Config listConfig;
    listConfig.col = LIST_START_COL;
    listConfig.row = LIST_START_ROW;
    listConfig.width = Renderer::GetDividerCol() - 1;
    listConfig.visibleRows = Renderer::GetFooterRow() - LIST_START_ROW - 1;
    listConfig.showScrollIndicators = true;

    sSettingsListState.itemCount = SETTINGS_ITEM_COUNT;

    UI::ListView::Render(sSettingsListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        const SettingItem& item = sSettingItems[index];
        view.prefix = isSelected ? "> " : "  ";

        // Build text based on setting type
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

    // --- Right side: Description ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Description:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "------------");

    // Show description for selected item
    int selectedIdx = UI::ListView::GetSelectedIndex(sSettingsListState);
    if (selectedIdx >= 0 && selectedIdx < SETTINGS_ITEM_COUNT) {
        const SettingItem& selected = sSettingItems[selectedIdx];
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, selected.descLine1);
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4, selected.descLine2);

        // Show action hint based on type
        const char* hint = "";
        switch (selected.type) {
            case SettingType::TOGGLE:     hint = "Press A to toggle"; break;
            case SettingType::COLOR:      hint = "Press A to edit"; break;
            case SettingType::BRIGHTNESS: hint = "Left/Right to adjust"; break;
            case SettingType::ACTION:     hint = "Press A to open"; break;
        }
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 6, hint);
    }

    // --- Footer ---
    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Edit %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      selectedIdx + 1, SETTINGS_ITEM_COUNT);
}

/**
 * Render the manage categories submenu.
 */
void renderManageCategories()
{
    // --- Header ---
    Renderer::DrawText(0, 0, "MANAGE CATEGORIES");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    // --- Divider ---
    drawDivider();

    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    // --- Left side: Category list using ListView ---
    UI::ListView::Config listConfig;
    listConfig.col = LIST_START_COL;
    listConfig.row = LIST_START_ROW;
    listConfig.width = Renderer::GetDividerCol() - 1;
    listConfig.visibleRows = CATEGORY_MANAGE_VISIBLE_ROWS;
    listConfig.showScrollIndicators = true;
    listConfig.canReorder = true;
    listConfig.canDelete = true;

    sManageCatsListState.itemCount = catCount;

    if (catCount == 0) {
        Renderer::DrawText(2, LIST_START_ROW, "(No categories)");
        Renderer::DrawTextF(2, LIST_START_ROW + 2, "Press %s to create one", Buttons::Actions::SETTINGS.label);
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

    // --- Right side: Actions ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Actions:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "--------");

    int selectedIdx = UI::ListView::GetSelectedIndex(sManageCatsListState);
    if (catCount == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, "No categories yet.");
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 5, "%s: Add New",
                          Buttons::Actions::SETTINGS.label);
    } else if (selectedIdx >= 0 && selectedIdx < catCount) {
        const Settings::Category& cat = categories[selectedIdx];
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, "Category: %s", cat.name);

        // Show visibility status
        const char* visStatus = cat.hidden ? "Hidden" : "Visible";
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4, "Status: %s", visStatus);

        // Action hints
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 6, "%s: Rename",
                          Buttons::Actions::CONFIRM.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 7, "%s: Delete",
                          Buttons::Actions::EDIT.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 8, "%s: %s",
                          Buttons::Actions::FAVORITE.label,
                          cat.hidden ? "Show" : "Hide");
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 9, "%s: Add New",
                          Buttons::Actions::SETTINGS.label);
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 11, "%s/%s: Move Up/Down",
                          Buttons::Actions::NAV_PAGE_UP.label,
                          Buttons::Actions::NAV_PAGE_DOWN.label);
    }

    // --- Footer ---
    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Rename %s:Back %s:Hide %s:Add  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      Buttons::Actions::FAVORITE.label,
                      Buttons::Actions::SETTINGS.label,
                      catCount > 0 ? selectedIdx + 1 : 0, catCount);
}

/**
 * Render color input mode.
 */
void renderColorInput()
{
    Renderer::DrawText(0, 0, "EDIT COLOR");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    const char* colorName = (sEditingSettingIndex >= 0) ? sSettingItems[sEditingSettingIndex].name : "Unknown";
    Renderer::DrawTextF(0, LIST_START_ROW, "Editing: %s", colorName);
    Renderer::DrawText(0, LIST_START_ROW + 2, "Enter RGBA hex value (8 digits):");

    // Render the input field
    sInputField.Render(0, LIST_START_ROW + 4);

    // Instructions
    Renderer::DrawText(0, LIST_START_ROW + 7, "Up/Down: Change character");
    Renderer::DrawTextF(0, LIST_START_ROW + 8, "%s/%s: Move cursor",
                      Buttons::Actions::INPUT_RIGHT.label,
                      Buttons::Actions::INPUT_LEFT.label);
    Renderer::DrawTextF(0, LIST_START_ROW + 9, "%s: Delete  %s: Confirm  %s: Cancel",
                      Buttons::Actions::INPUT_DELETE.label,
                      Buttons::Actions::INPUT_CONFIRM.label,
                      Buttons::Actions::INPUT_CANCEL.label);
}

/**
 * Render category name input mode.
 */
void renderNameInput()
{
    Renderer::DrawText(0, 0, "CATEGORY NAME");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    if (sEditingCategoryId < 0) {
        Renderer::DrawText(0, LIST_START_ROW, "Enter name for new category:");
    } else {
        Renderer::DrawText(0, LIST_START_ROW, "Enter new name:");
    }

    // Render the input field
    sInputField.Render(0, LIST_START_ROW + 2);

    // Instructions
    Renderer::DrawText(0, LIST_START_ROW + 5, "Up/Down: Change character");
    Renderer::DrawTextF(0, LIST_START_ROW + 6, "%s/%s: Move cursor",
                      Buttons::Actions::INPUT_RIGHT.label,
                      Buttons::Actions::INPUT_LEFT.label);
    Renderer::DrawTextF(0, LIST_START_ROW + 7, "%s: Delete  %s: Confirm  %s: Cancel",
                      Buttons::Actions::INPUT_DELETE.label,
                      Buttons::Actions::INPUT_CONFIRM.label,
                      Buttons::Actions::INPUT_CANCEL.label);
}

/**
 * Render the system apps submenu.
 */
void renderSystemApps()
{
    // --- Header ---
    Renderer::DrawText(0, 0, "SYSTEM APPS");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    // --- Divider ---
    drawDivider();

    // --- Left side: System app list using ListView ---
    UI::ListView::Config listConfig;
    listConfig.col = LIST_START_COL;
    listConfig.row = LIST_START_ROW;
    listConfig.width = Renderer::GetDividerCol() - 1;
    listConfig.visibleRows = Renderer::GetFooterRow() - LIST_START_ROW - 1;
    listConfig.showScrollIndicators = true;

    sSystemAppsListState.itemCount = SYSTEM_APP_COUNT;

    UI::ListView::Render(sSystemAppsListState, listConfig, [](int index, bool isSelected) {
        UI::ListView::ItemView view;
        view.text = sSystemApps[index].name;
        view.prefix = isSelected ? "> " : "  ";
        return view;
    });

    // --- Right side: Description ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Description:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "------------");

    // Show description for selected app
    int selectedIdx = UI::ListView::GetSelectedIndex(sSystemAppsListState);
    if (selectedIdx >= 0 && selectedIdx < SYSTEM_APP_COUNT) {
        const SystemAppOption& app = sSystemApps[selectedIdx];
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, app.description);
    }

    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 5, "Press A to launch");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 7, "Note: The game will be");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 8, "suspended while the");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 9, "system app is open.");

    // --- Footer ---
    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Launch %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      selectedIdx + 1, SYSTEM_APP_COUNT);
}

/**
 * Render the settings mode screen.
 */
void renderSettingsMode()
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
        case SettingsSubMode::COLOR_INPUT:
            renderColorInput();
            break;
        case SettingsSubMode::NAME_INPUT:
            renderNameInput();
            break;
    }
}

/**
 * Handle main settings list input.
 */
void handleSettingsMainInput(uint32_t pressed)
{
    // Configure ListView for navigation
    UI::ListView::Config listConfig;
    listConfig.visibleRows = Renderer::GetFooterRow() - LIST_START_ROW - 1;
    listConfig.canConfirm = true;
    listConfig.canCancel = true;
    // Disable skip navigation - we use Left/Right for brightness adjustment
    listConfig.smallSkip = 1;  // Make Left/Right act like Up/Down

    sSettingsListState.itemCount = SETTINGS_ITEM_COUNT;

    // Handle navigation
    UI::ListView::HandleInput(sSettingsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sSettingsListState);

    // Check if current setting is brightness for left/right handling
    if (selectedIdx >= 0 && selectedIdx < SETTINGS_ITEM_COUNT) {
        const SettingItem& currentItem = sSettingItems[selectedIdx];
        if (currentItem.type == SettingType::BRIGHTNESS) {
            // Left/Right to adjust brightness (handled separately from navigation)
            if (Buttons::Actions::NAV_SKIP_UP.Pressed(pressed)) {
                int brightness = getCurrentBrightness();
                if (brightness > 1) {
                    setBrightness(brightness - 1);
                }
            }
            if (Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed)) {
                int brightness = getCurrentBrightness();
                if (brightness < 5) {
                    setBrightness(brightness + 1);
                }
            }
        }
    }

    // Handle actions
    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    if (action == UI::ListView::Action::CONFIRM && selectedIdx >= 0) {
        const SettingItem& item = sSettingItems[selectedIdx];

        switch (item.type) {
            case SettingType::TOGGLE: {
                // Toggle the boolean value
                bool* value = getTogglePtr(item.dataOffset);
                *value = !(*value);
                break;
            }
            case SettingType::COLOR: {
                // Edit color - initialize input field with current value
                sEditingSettingIndex = selectedIdx;
                uint32_t* color = getColorPtr(item.dataOffset);
                char hexStr[16];
                snprintf(hexStr, sizeof(hexStr), "%08X", *color);
                sInputField.Init(8, TextInput::Library::HEX);
                sInputField.SetValue(hexStr);
                sSettingsSubMode = SettingsSubMode::COLOR_INPUT;
                break;
            }
            case SettingType::BRIGHTNESS: {
                // A toggles between min and max brightness
                int brightness = getCurrentBrightness();
                setBrightness(brightness >= 3 ? 1 : 5);
                break;
            }
            case SettingType::ACTION: {
                // Handle action based on action ID
                if (item.dataOffset == ACTION_MANAGE_CATEGORIES) {
                    sManageCatsListState = UI::ListView::State();  // Reset list state
                    sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
                } else if (item.dataOffset == ACTION_SYSTEM_APPS) {
                    sSystemAppsListState = UI::ListView::State();  // Reset state
                    sSettingsSubMode = SettingsSubMode::SYSTEM_APPS;
                } else if (item.dataOffset == ACTION_DEBUG_GRID) {
                    sCurrentMode = Mode::DEBUG_GRID;
                }
                break;
            }
        }
    }

    // Back
    if (action == UI::ListView::Action::CANCEL) {
        Settings::Save();
        sCurrentMode = Mode::BROWSE;
    }
}

/**
 * Handle manage categories input.
 */
void handleManageCategoriesInput(uint32_t pressed)
{
    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    // Configure ListView
    UI::ListView::Config listConfig;
    listConfig.visibleRows = CATEGORY_MANAGE_VISIBLE_ROWS;
    listConfig.canConfirm = true;
    listConfig.canCancel = true;
    listConfig.canReorder = true;
    listConfig.canDelete = true;
    listConfig.canFavorite = true;  // Y for toggle visibility

    sManageCatsListState.itemCount = catCount;

    // Handle navigation
    UI::ListView::HandleInput(sManageCatsListState, pressed, listConfig);
    int selectedIdx = UI::ListView::GetSelectedIndex(sManageCatsListState);

    // Add new category (+) - handled separately
    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        sEditingCategoryId = -1;  // -1 = new
        sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
        sInputField.Clear();
        sSettingsSubMode = SettingsSubMode::NAME_INPUT;
        return;
    }

    // Handle actions
    UI::ListView::Action action = UI::ListView::GetAction(pressed, listConfig);
    switch (action) {
        case UI::ListView::Action::CONFIRM:
            // Rename selected category
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount) {
                const Settings::Category& cat = categories[selectedIdx];
                sEditingCategoryId = cat.id;
                sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
                sInputField.SetValue(cat.name);
                sSettingsSubMode = SettingsSubMode::NAME_INPUT;
            }
            break;

        case UI::ListView::Action::DELETE:
            // Delete category
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::DeleteCategory(catId);
                // Update item count and clamp selection
                sManageCatsListState.SetItemCount(Settings::GetCategoryCount(), listConfig.visibleRows);
                Categories::RefreshFilter();
            }
            break;

        case UI::ListView::Action::FAVORITE:
            // Toggle visibility
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount) {
                uint16_t catId = categories[selectedIdx].id;
                bool currentlyHidden = Settings::IsCategoryHidden(catId);
                Settings::SetCategoryHidden(catId, !currentlyHidden);
            }
            break;

        case UI::ListView::Action::MOVE_UP:
            // Move category up
            if (catCount > 0 && selectedIdx > 0 && selectedIdx < catCount) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::MoveCategoryUp(catId);
                // Follow the moved category
                sManageCatsListState.selectedIndex--;
            }
            break;

        case UI::ListView::Action::MOVE_DOWN:
            // Move category down
            if (catCount > 0 && selectedIdx >= 0 && selectedIdx < catCount - 1) {
                uint16_t catId = categories[selectedIdx].id;
                Settings::MoveCategoryDown(catId);
                // Follow the moved category
                sManageCatsListState.selectedIndex++;
            }
            break;

        case UI::ListView::Action::CANCEL:
            // Back
            sSettingsSubMode = SettingsSubMode::MAIN;
            break;

        default:
            break;
    }
}

/**
 * Handle color input.
 */
void handleColorInputInput(uint32_t pressed, uint32_t held)
{
    TextInput::Result result = sInputField.HandleInput(pressed, held);

    if (result == TextInput::Result::CONFIRMED) {
        // Parse the hex value
        char hexStr[16];
        sInputField.GetValue(hexStr, sizeof(hexStr));

        // Convert to uint32_t
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

        // Set the color using the data-driven system
        if (sEditingSettingIndex >= 0) {
            const SettingItem& item = sSettingItems[sEditingSettingIndex];
            if (item.type == SettingType::COLOR) {
                uint32_t* color = getColorPtr(item.dataOffset);
                *color = value;
            }
        }

        sEditingSettingIndex = -1;
        sSettingsSubMode = SettingsSubMode::MAIN;
    } else if (result == TextInput::Result::CANCELLED) {
        sEditingSettingIndex = -1;
        sSettingsSubMode = SettingsSubMode::MAIN;
    }
}

/**
 * Handle name input.
 */
void handleNameInputInput(uint32_t pressed, uint32_t held)
{
    TextInput::Result result = sInputField.HandleInput(pressed, held);

    if (result == TextInput::Result::CONFIRMED) {
        char name[Settings::MAX_CATEGORY_NAME];
        sInputField.GetValue(name, sizeof(name));

        // Only proceed if name is not empty
        if (name[0] != '\0') {
            if (sEditingCategoryId < 0) {
                // Create new category
                Settings::CreateCategory(name);
            } else {
                // Rename existing category
                Settings::RenameCategory(static_cast<uint16_t>(sEditingCategoryId), name);
            }

            // Refresh category filter
            Categories::RefreshFilter();
        }

        sEditingCategoryId = -1;
        sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
    } else if (result == TextInput::Result::CANCELLED) {
        sEditingCategoryId = -1;
        sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
    }
}

/**
 * Launch a system app by ID.
 * Closes the menu and launches the requested app.
 */
void launchSystemApp(int appId)
{
    // Close the menu first
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
            // Standard system app - use _SYSLaunchSettings or SYSLaunchMiiStudio
            if (appId == SYSTEM_APP_ID_MII_MAKER) {
                SYSLaunchMiiStudio(nullptr);
            } else if (appId == SYSTEM_APP_ID_SYSTEM_SETTINGS) {
                _SYSLaunchSettings(nullptr);
            } else if (appId == SYSTEM_APP_ID_PARENTAL_CONTROLS) {
                _SYSLaunchParental(nullptr);
            } else if (appId == SYSTEM_APP_ID_NOTIFICATIONS) {
                _SYSLaunchNotifications(nullptr);
            } else {
                // Try generic title launch for other system apps
                // Note: This may not work for all apps
                SYSLaunchMenu();  // Fallback to menu
            }
            break;
    }
}

/**
 * Handle system apps submenu input.
 */
void handleSystemAppsInput(uint32_t pressed)
{
    // Configure ListView for input handling
    UI::ListView::Config listConfig;
    listConfig.visibleRows = Renderer::GetFooterRow() - LIST_START_ROW - 1;
    listConfig.canConfirm = true;
    listConfig.canCancel = true;

    sSystemAppsListState.itemCount = SYSTEM_APP_COUNT;

    // Handle navigation
    UI::ListView::HandleInput(sSystemAppsListState, pressed, listConfig);

    // Handle actions
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

/**
 * Handle input in settings mode.
 * @param pressed Buttons just pressed this frame
 * @param held    Buttons held this frame (for text input repeat)
 */
void handleSettingsModeInput(uint32_t pressed, uint32_t held)
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
        case SettingsSubMode::COLOR_INPUT:
            handleColorInputInput(pressed, held);
            break;
        case SettingsSubMode::NAME_INPUT:
            handleNameInputInput(pressed, held);
            break;
    }
}

// =============================================================================
// Debug Grid Mode
// =============================================================================

/**
 * Render debug grid overlay.
 * Shows character grid with position labels and pixel information.
 */
void renderDebugGridMode()
{
    int gridWidth = Renderer::GetGridWidth();
    int gridHeight = Renderer::GetGridHeight();
    int screenWidth = Renderer::GetScreenWidth();
    int screenHeight = Renderer::GetScreenHeight();

    // Draw header info
    Renderer::DrawTextF(0, 0, 0xFFFF00FF, "DEBUG GRID - Screen: %dx%d px  Grid: %dx%d chars",
                        screenWidth, screenHeight, gridWidth, gridHeight);

    // Draw column markers every 10 columns
    for (int col = 0; col < gridWidth; col += 10) {
        Renderer::DrawTextF(col, 1, 0x00FF00FF, "%d", col);
    }

    // Draw horizontal line at row 2
    for (int col = 0; col < gridWidth; col++) {
        Renderer::DrawText(col, 2, "-", 0x888888FF);
    }

    // Draw row markers and grid lines
    for (int row = 3; row < gridHeight - 1; row++) {
        // Row number on left
        Renderer::DrawTextF(0, row, 0x00FF00FF, "%2d", row);

        // Vertical lines at key positions
        int dividerCol = Renderer::GetDividerCol();
        int detailsCol = Renderer::GetDetailsPanelCol();

        // Mark divider column
        Renderer::DrawText(dividerCol, row, "|", 0xFF0000FF);

        // Mark details panel start
        if (detailsCol < gridWidth) {
            Renderer::DrawText(detailsCol, row, ">", 0x00FFFFFF);
        }

        // Light grid every 10 columns
        for (int col = 10; col < gridWidth; col += 10) {
            if (col != dividerCol && col != detailsCol) {
                Renderer::DrawText(col, row, ".", 0x444444FF);
            }
        }
    }

    // Draw footer with layout info
    int footerRow = gridHeight - 1;
    int dividerCol = Renderer::GetDividerCol();
    int detailsCol = Renderer::GetDetailsPanelCol();
    int visibleRows = Renderer::GetVisibleRows();

    Renderer::DrawTextF(0, footerRow, 0xFFFFFFFF,
                        "Divider:%d Details:%d VisRows:%d  B:Back",
                        dividerCol, detailsCol, visibleRows);
}

/**
 * Handle input in debug grid mode.
 */
void handleDebugGridModeInput(uint32_t pressed)
{
    // B to go back to settings
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        sCurrentMode = Mode::SETTINGS;
    }
}

/**
 * Main menu loop.
 * @return Title ID to launch, or 0 if cancelled
 */
uint64_t runMenuLoop()
{
    uint64_t titleToLaunch = 0;
    VPADStatus vpadStatus;
    VPADReadError vpadError;

    while (sIsOpen) {
        // Begin frame (waits for vsync, clears screen)
        Renderer::BeginFrame(Settings::Get().bgColor);

        // Render based on current mode
        switch (sCurrentMode) {
            case Mode::BROWSE:
                renderBrowseMode();
                break;
            case Mode::EDIT:
                renderEditMode();
                break;
            case Mode::SETTINGS:
                renderSettingsMode();
                break;
            case Mode::DEBUG_GRID:
                renderDebugGridMode();
                break;
        }

        // Process image loading queue
        ImageLoader::Update();

        // End frame (flush and flip)
        Renderer::EndFrame();

        // Read input
        int32_t readResult = VPADRead(VPAD_CHAN_0, &vpadStatus, 1, &vpadError);

        if (readResult > 0 && vpadError == VPAD_READ_SUCCESS) {
            uint32_t pressed = vpadStatus.trigger;
            uint32_t held = vpadStatus.hold;

            // Handle input based on current mode
            switch (sCurrentMode) {
                case Mode::BROWSE:
                    titleToLaunch = handleBrowseModeInput(pressed);
                    break;
                case Mode::EDIT:
                    handleEditModeInput(pressed);
                    break;
                case Mode::SETTINGS:
                    handleSettingsModeInput(pressed, held);
                    break;
                case Mode::DEBUG_GRID:
                    handleDebugGridModeInput(pressed);
                    break;
            }
        }
    }

    return titleToLaunch;
}

} // anonymous namespace

// =============================================================================
// Public Implementation
// =============================================================================

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
    // Already open?
    if (sIsOpen) {
        return false;
    }

    // Check startup grace period
    if (sApplicationStartTime != 0) {
        OSTime now = OSGetTime();
        OSTime elapsed = now - sApplicationStartTime;
        uint32_t elapsedMs = static_cast<uint32_t>(OSTicksToMilliseconds(elapsed));

        if (elapsedMs < STARTUP_GRACE_MS) {
            return false;
        }
    }

    // Check foreground state
    if (!sInForeground) {
        return false;
    }

    // Check ProcUI state
    if (!ProcUIIsRunning()) {
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

    // Initialize screen rendering
    if (!Renderer::Init()) {
        // Screen init failed - can't open menu
        return;
    }

    // Initialize image loader for title icons
    ImageLoader::Init();

    // Ensure titles are loaded
    Titles::Load();

    // Initialize category filter
    Categories::Init();

    // Restore last selection from settings
    sTitleListState = UI::ListView::State();  // Reset state
    sTitleListState.selectedIndex = Settings::Get().lastIndex;
    clampSelection();

    // Mark as open and reset mode
    sIsOpen = true;
    sCurrentMode = Mode::BROWSE;

    // Run the menu loop (blocks until menu closes)
    uint64_t titleToLaunch = runMenuLoop();

    // Save state
    Settings::Get().lastIndex = sTitleListState.selectedIndex;
    Settings::Save();

    // Cleanup
    sIsOpen = false;
    ImageLoader::Shutdown();
    Renderer::Shutdown();

    // Launch selected title if any
    if (titleToLaunch != 0) {
        SYSLaunchTitle(titleToLaunch);
    }
}

void Close()
{
    sIsOpen = false;
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
    sInForeground = false;

    if (sIsOpen) {
        Close();
    }
}

} // namespace Menu
