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

// Wii U SDK headers
#include <vpad/input.h>           // VPADRead, VPADStatus
#include <sysapp/launch.h>        // SYSLaunchTitle
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
int sSelectedIndex = 0;     // Currently selected title index
int sScrollOffset = 0;      // First visible title index

// Edit mode state
int sEditCategoryIndex = 0;      // Currently selected category in edit mode
int sEditCategoryScroll = 0;     // Scroll offset for category list

// Settings mode state
enum class SettingsSubMode {
    MAIN,           // Main settings list
    MANAGE_CATS,    // Category management submenu
    COLOR_INPUT,    // Editing a color value
    NAME_INPUT      // Editing a category name
};

// =============================================================================
// Data-Driven Settings System
// =============================================================================

enum class SettingType {
    TOGGLE,     // Boolean on/off
    COLOR,      // RGBA hex color
    ACTION      // Opens a submenu or performs an action
};

enum SettingAction {
    ACTION_MANAGE_CATEGORIES = -1
};

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
#define ACTION_SETTING(n, d1, d2, actionId) \
    {n, d1, d2, SettingType::ACTION, actionId}

// All settings defined in one place - easy to add/remove/reorder
static const SettingItem sSettingItems[] = {
    TOGGLE_SETTING("Show Numbers",    "Show line numbers before",    "each title in the list.",    showNumbers),
    TOGGLE_SETTING("Show Favorites",  "Show favorite marker (*)",    "in the title list.",         showFavorites),
    COLOR_SETTING("Background",       "Menu background color.",      "RGBA hex format.",           bgColor),
    COLOR_SETTING("Title Text",       "Normal title text color.",    "RGBA hex format.",           titleColor),
    COLOR_SETTING("Highlighted",      "Selected title color.",       "RGBA hex format.",           highlightedTitleColor),
    COLOR_SETTING("Favorite",         "Favorite marker color.",      "RGBA hex format.",           favoriteColor),
    COLOR_SETTING("Header",           "Header text color.",          "RGBA hex format.",           headerColor),
    COLOR_SETTING("Category",         "Category tab color.",         "RGBA hex format.",           categoryColor),
    ACTION_SETTING("Manage Categories", "Create, rename, or delete", "custom categories.",         ACTION_MANAGE_CATEGORIES),
};

static constexpr int SETTINGS_ITEM_COUNT = sizeof(sSettingItems) / sizeof(sSettingItems[0]);

int sSettingsIndex = 0;              // Currently selected setting
SettingsSubMode sSettingsSubMode = SettingsSubMode::MAIN;
int sManageCatIndex = 0;             // Selected category in manage mode
int sManageCatScroll = 0;            // Scroll offset in manage mode
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
// Helper Functions
// =============================================================================

/**
 * Clamp selection to valid range and update scroll offset.
 */
void clampSelection()
{
    int count = Categories::GetFilteredCount();

    // Clamp selection to valid range
    if (sSelectedIndex < 0) {
        sSelectedIndex = 0;
    }
    if (sSelectedIndex >= count) {
        sSelectedIndex = count > 0 ? count - 1 : 0;
    }

    // Update scroll offset to keep selection visible
    if (sSelectedIndex < sScrollOffset) {
        sScrollOffset = sSelectedIndex;
    }
    if (sSelectedIndex >= sScrollOffset + Renderer::GetVisibleRows()) {
        sScrollOffset = sSelectedIndex - Renderer::GetVisibleRows() + 1;
    }

    // Clamp scroll offset
    if (sScrollOffset < 0) {
        sScrollOffset = 0;
    }
    int maxScroll = count - Renderer::GetVisibleRows();
    if (maxScroll < 0) maxScroll = 0;
    if (sScrollOffset > maxScroll) {
        sScrollOffset = maxScroll;
    }
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

    if (count == 0) {
        Renderer::DrawText(2, LIST_START_ROW + 2, "No titles in this category");
        return;
    }

    // Check if we should show numbers
    bool showNums = Settings::Get().showNumbers;

    // Calculate max name width based on whether numbers are shown
    int maxNameLen = Renderer::GetTitleNameWidth(showNums);

    // Draw visible titles
    for (int i = 0; i < Renderer::GetVisibleRows() && (sScrollOffset + i) < count; i++) {
        int idx = sScrollOffset + i;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);

        if (!title) continue;

        char line[48];

        // Truncate name to fit
        char displayName[32];
        strncpy(displayName, title->name, maxNameLen);
        displayName[maxNameLen] = '\0';

        const char* cursor = (idx == sSelectedIndex) ? ">" : " ";

        // Check if we should show favorite markers
        bool showFavs = Settings::Get().showFavorites;
        const char* favMark = "";
        if (showFavs) {
            bool isFav = Settings::IsFavorite(title->titleId);
            favMark = isFav ? "* " : "  ";
        }

        if (showNums) {
            // Format: "> 001. * Name" or "> 001. Name"
            snprintf(line, sizeof(line), "%s%3d. %s%-*s",
                     cursor, idx + 1, favMark, maxNameLen, displayName);
        } else {
            // Format: "> * Name" or "> Name"
            snprintf(line, sizeof(line), "%s %s%-*s",
                     cursor, favMark, maxNameLen, displayName);
        }

        Renderer::DrawText(LIST_START_COL, LIST_START_ROW + i, line);
    }

    // Scroll indicators
    if (sScrollOffset > 0) {
        Renderer::DrawText(Renderer::GetListWidth() - 4, LIST_START_ROW, "[UP]");
    }
    if (sScrollOffset + Renderer::GetVisibleRows() < count) {
        Renderer::DrawText(Renderer::GetListWidth() - 6, LIST_START_ROW + Renderer::GetVisibleRows() - 1, "[DOWN]");
    }
}

/**
 * Draw the details panel on the right side.
 */
void drawDetailsPanel()
{
    int count = Categories::GetFilteredCount();
    if (count == 0 || sSelectedIndex < 0 || sSelectedIndex >= count) {
        return;
    }

    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
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

    // Bottom row: main controls with customizable button labels
    char footer[100];
    snprintf(footer, sizeof(footer),
             "%s:Go %s:Close %s:Fav %s:Edit %s:Settings ZL/ZR:Cat [%d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             Buttons::Actions::FAVORITE.label,
             Buttons::Actions::EDIT.label,
             Buttons::Actions::SETTINGS.label,
             sSelectedIndex + 1,
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

    // Navigation - Up/Down
    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        if (sSelectedIndex > 0) {
            sSelectedIndex--;
            clampSelection();
        }
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        if (sSelectedIndex < count - 1) {
            sSelectedIndex++;
            clampSelection();
        }
    }

    // Navigation - Small skip (Left/Right)
    if (Buttons::Actions::NAV_SKIP_UP.Pressed(pressed)) {
        sSelectedIndex -= Buttons::Skip::SMALL;
        clampSelection();
    }
    if (Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed)) {
        sSelectedIndex += Buttons::Skip::SMALL;
        clampSelection();
    }

    // Navigation - Large skip (L/R)
    if (Buttons::Actions::NAV_PAGE_UP.Pressed(pressed)) {
        sSelectedIndex -= Buttons::Skip::LARGE;
        clampSelection();
    }
    if (Buttons::Actions::NAV_PAGE_DOWN.Pressed(pressed)) {
        sSelectedIndex += Buttons::Skip::LARGE;
        clampSelection();
    }

    // Category navigation (ZL/ZR)
    if (Buttons::Actions::CATEGORY_PREV.Pressed(pressed)) {
        Categories::PreviousCategory();
        sSelectedIndex = 0;
        sScrollOffset = 0;
    }
    if (Buttons::Actions::CATEGORY_NEXT.Pressed(pressed)) {
        Categories::NextCategory();
        sSelectedIndex = 0;
        sScrollOffset = 0;
    }

    // Toggle favorite (Y)
    if (Buttons::Actions::FAVORITE.Pressed(pressed)) {
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
        if (title) {
            Settings::ToggleFavorite(title->titleId);
            // Re-filter in case we're in Favorites category
            Categories::RefreshFilter();
            clampSelection();
        }
    }

    // Edit mode (X)
    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        // Reset edit mode state
        sEditCategoryIndex = 0;
        sEditCategoryScroll = 0;
        sCurrentMode = Mode::EDIT;
    }

    // Settings (Plus)
    if (Buttons::Actions::SETTINGS.Pressed(pressed)) {
        // Reset settings mode state
        sSettingsIndex = 0;
        sSettingsSubMode = SettingsSubMode::MAIN;
        sCurrentMode = Mode::SETTINGS;
    }

    // Cancel/Close (B)
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        sIsOpen = false;
        return 0;
    }

    // Confirm/Launch (A)
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
        if (title) {
            sIsOpen = false;
            return title->titleId;
        }
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
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
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

    // --- Right side: Category checkboxes ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Categories:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "------------");

    // Get user-defined categories
    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    if (catCount == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3,
                         "No categories defined.");
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4,
                         "Create in Settings (+)");
    } else {
        // Calculate visible rows for categories
        constexpr int CAT_VISIBLE_ROWS = 10;
        int startRow = LIST_START_ROW + 3;

        // Clamp scroll offset
        if (sEditCategoryScroll > catCount - CAT_VISIBLE_ROWS) {
            sEditCategoryScroll = catCount - CAT_VISIBLE_ROWS;
        }
        if (sEditCategoryScroll < 0) sEditCategoryScroll = 0;

        // Clamp selection
        if (sEditCategoryIndex < 0) sEditCategoryIndex = 0;
        if (sEditCategoryIndex >= catCount) sEditCategoryIndex = catCount - 1;

        // Ensure selection is visible
        if (sEditCategoryIndex < sEditCategoryScroll) {
            sEditCategoryScroll = sEditCategoryIndex;
        }
        if (sEditCategoryIndex >= sEditCategoryScroll + CAT_VISIBLE_ROWS) {
            sEditCategoryScroll = sEditCategoryIndex - CAT_VISIBLE_ROWS + 1;
        }

        // Draw category list
        for (int i = 0; i < CAT_VISIBLE_ROWS && (sEditCategoryScroll + i) < catCount; i++) {
            int idx = sEditCategoryScroll + i;
            const Settings::Category& cat = categories[idx];

            // Check if title is in this category
            bool inCategory = Settings::TitleHasCategory(title->titleId, cat.id);

            // Cursor and checkbox
            const char* cursor = (idx == sEditCategoryIndex) ? ">" : " ";
            const char* checkbox = inCategory ? "[X]" : "[ ]";

            Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), startRow + i,
                              "%s %s %s", cursor, checkbox, cat.name);
        }

        // Scroll indicators
        if (sEditCategoryScroll > 0) {
            Renderer::DrawText(Renderer::GetDetailsPanelCol() + 20, startRow, "[UP]");
        }
        if (sEditCategoryScroll + CAT_VISIBLE_ROWS < catCount) {
            Renderer::DrawText(Renderer::GetDetailsPanelCol() + 18, startRow + CAT_VISIBLE_ROWS - 1, "[DOWN]");
        }
    }

    // --- Footer ---
    char footer[80];
    snprintf(footer, sizeof(footer),
             "%s:Toggle %s:Back  [Category %d/%d]",
             Buttons::Actions::CONFIRM.label,
             Buttons::Actions::CANCEL.label,
             sEditCategoryIndex + 1,
             catCount > 0 ? catCount : 1);
    Renderer::DrawText(0, Renderer::GetFooterRow(), footer);
}

/**
 * Handle input in edit mode.
 * @param pressed Buttons just pressed this frame
 */
void handleEditModeInput(uint32_t pressed)
{
    const Titles::TitleInfo* title = Categories::GetFilteredTitle(sSelectedIndex);
    if (!title) {
        // No title selected - go back
        sCurrentMode = Mode::BROWSE;
        return;
    }

    int catCount = Settings::GetCategoryCount();

    // Navigation - Up/Down through categories
    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        if (sEditCategoryIndex > 0) {
            sEditCategoryIndex--;
        }
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        if (sEditCategoryIndex < catCount - 1) {
            sEditCategoryIndex++;
        }
    }

    // Toggle category (A)
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (catCount > 0 && sEditCategoryIndex >= 0 && sEditCategoryIndex < catCount) {
            const auto& categories = Settings::Get().categories;
            uint16_t catId = categories[sEditCategoryIndex].id;

            if (Settings::TitleHasCategory(title->titleId, catId)) {
                Settings::RemoveTitleFromCategory(title->titleId, catId);
            } else {
                Settings::AssignTitleToCategory(title->titleId, catId);
            }

            // Refresh the filter in case this affects the current view
            Categories::RefreshFilter();
        }
    }

    // Cancel/Back (B)
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        // Save changes before exiting
        Settings::Save();
        sCurrentMode = Mode::BROWSE;
    }
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

    // --- Left side: Settings list (data-driven) ---
    int row = LIST_START_ROW;

    for (int i = 0; i < SETTINGS_ITEM_COUNT && row < Renderer::GetFooterRow() - 1; i++) {
        const SettingItem& item = sSettingItems[i];
        const char* cursor = (sSettingsIndex == i) ? ">" : " ";

        switch (item.type) {
            case SettingType::TOGGLE: {
                bool value = *getTogglePtr(item.dataOffset);
                Renderer::DrawTextF(0, row++, "%s %s: %s", cursor, item.name, value ? "ON" : "OFF");
                break;
            }
            case SettingType::COLOR: {
                uint32_t value = *getColorPtr(item.dataOffset);
                Renderer::DrawTextF(0, row++, "%s %s: %08X", cursor, item.name, value);
                break;
            }
            case SettingType::ACTION: {
                if (item.dataOffset == ACTION_MANAGE_CATEGORIES) {
                    Renderer::DrawTextF(0, row++, "%s %s (%d)", cursor, item.name, Settings::GetCategoryCount());
                } else {
                    Renderer::DrawTextF(0, row++, "%s %s", cursor, item.name);
                }
                break;
            }
        }
    }

    // --- Right side: Description ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Description:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "------------");

    // Show description for selected item
    const SettingItem& selected = sSettingItems[sSettingsIndex];
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, selected.descLine1);
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4, selected.descLine2);

    // Show action hint based on type
    const char* hint = "";
    switch (selected.type) {
        case SettingType::TOGGLE: hint = "Press A to toggle"; break;
        case SettingType::COLOR:  hint = "Press A to edit"; break;
        case SettingType::ACTION: hint = "Press A to open"; break;
    }
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 6, hint);

    // --- Footer ---
    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Edit %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      sSettingsIndex + 1, SETTINGS_ITEM_COUNT);
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

    // --- Left side: Category list ---
    constexpr int CAT_VISIBLE_ROWS = 12;
    int row = LIST_START_ROW;

    // "+ Add New" always at top
    const char* cursor = (sManageCatIndex == 0) ? ">" : " ";
    Renderer::DrawTextF(0, row++, "%s + Add New Category", cursor);

    row++;  // Blank line

    // Existing categories
    if (catCount == 0) {
        Renderer::DrawText(2, row, "(No categories)");
    } else {
        // Clamp scroll and selection
        if (sManageCatScroll > catCount - CAT_VISIBLE_ROWS + 2) {
            sManageCatScroll = catCount - CAT_VISIBLE_ROWS + 2;
        }
        if (sManageCatScroll < 0) sManageCatScroll = 0;

        // Selection is offset by 1 (0 = Add New)
        int maxSel = catCount;
        if (sManageCatIndex < 0) sManageCatIndex = 0;
        if (sManageCatIndex > maxSel) sManageCatIndex = maxSel;

        // Draw categories
        for (int i = 0; i < CAT_VISIBLE_ROWS - 2 && (sManageCatScroll + i) < catCount; i++) {
            int idx = sManageCatScroll + i;
            const Settings::Category& cat = categories[idx];

            // Selection index is offset by 1
            cursor = (sManageCatIndex == idx + 1) ? ">" : " ";

            // Show hidden status
            if (cat.hidden) {
                Renderer::DrawTextF(0, row + i, "%s %s (hidden)", cursor, cat.name);
            } else {
                Renderer::DrawTextF(0, row + i, "%s %s", cursor, cat.name);
            }
        }
    }

    // --- Right side: Actions ---
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW, "Actions:");
    Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 1, "--------");

    if (sManageCatIndex == 0) {
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 3, "Create a new category");
        Renderer::DrawText(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 4, "for organizing titles.");
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 6, "%s: Create",
                          Buttons::Actions::CONFIRM.label);
    } else if (sManageCatIndex <= catCount) {
        const Settings::Category& cat = categories[sManageCatIndex - 1];
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
        Renderer::DrawTextF(Renderer::GetDetailsPanelCol(), LIST_START_ROW + 10, "%s/%s: Move Up/Down",
                          Buttons::Actions::NAV_PAGE_UP.label,
                          Buttons::Actions::NAV_PAGE_DOWN.label);
    }

    // --- Footer ---
    Renderer::DrawTextF(0, Renderer::GetFooterRow(), "%s:Select %s:Back %s:Hide/Show L/R:Move  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      Buttons::Actions::FAVORITE.label,
                      sManageCatIndex + 1, catCount + 1);
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
    // Navigation
    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        if (sSettingsIndex > 0) sSettingsIndex--;
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        if (sSettingsIndex < SETTINGS_ITEM_COUNT - 1) sSettingsIndex++;
    }

    // Confirm/Edit - data-driven handling
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        const SettingItem& item = sSettingItems[sSettingsIndex];

        switch (item.type) {
            case SettingType::TOGGLE: {
                // Toggle the boolean value
                bool* value = getTogglePtr(item.dataOffset);
                *value = !(*value);
                break;
            }
            case SettingType::COLOR: {
                // Edit color - initialize input field with current value
                sEditingSettingIndex = sSettingsIndex;
                uint32_t* color = getColorPtr(item.dataOffset);
                char hexStr[16];
                snprintf(hexStr, sizeof(hexStr), "%08X", *color);
                sInputField.Init(8, TextInput::Library::HEX);
                sInputField.SetValue(hexStr);
                sSettingsSubMode = SettingsSubMode::COLOR_INPUT;
                break;
            }
            case SettingType::ACTION: {
                // Handle action based on action ID
                if (item.dataOffset == ACTION_MANAGE_CATEGORIES) {
                    sManageCatIndex = 0;
                    sManageCatScroll = 0;
                    sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
                }
                break;
            }
        }
    }

    // Back
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
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
    int maxIndex = catCount;  // 0 = Add New, 1..catCount = categories

    // Navigation
    if (Buttons::Actions::NAV_UP.Pressed(pressed)) {
        if (sManageCatIndex > 0) sManageCatIndex--;
    }
    if (Buttons::Actions::NAV_DOWN.Pressed(pressed)) {
        if (sManageCatIndex < maxIndex) sManageCatIndex++;
    }

    // Confirm (A) - Add new or rename
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (sManageCatIndex == 0) {
            // Add new category
            sEditingCategoryId = -1;  // -1 = new
            sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
            sInputField.Clear();
            sSettingsSubMode = SettingsSubMode::NAME_INPUT;
        } else if (sManageCatIndex <= catCount) {
            // Rename existing category
            const auto& categories = Settings::Get().categories;
            const Settings::Category& cat = categories[sManageCatIndex - 1];
            sEditingCategoryId = cat.id;
            sInputField.Init(Settings::MAX_CATEGORY_NAME - 1, TextInput::Library::ALPHA_NUMERIC);
            sInputField.SetValue(cat.name);
            sSettingsSubMode = SettingsSubMode::NAME_INPUT;
        }
    }

    // Edit (X) - Delete category
    if (Buttons::Actions::EDIT.Pressed(pressed)) {
        if (sManageCatIndex > 0 && sManageCatIndex <= catCount) {
            const auto& categories = Settings::Get().categories;
            uint16_t catId = categories[sManageCatIndex - 1].id;
            Settings::DeleteCategory(catId);

            // Adjust selection if needed
            int newCount = Settings::GetCategoryCount();
            if (sManageCatIndex > newCount) {
                sManageCatIndex = newCount;
            }

            // Refresh category filter
            Categories::RefreshFilter();
        }
    }

    // Favorite (Y) - Toggle visibility
    if (Buttons::Actions::FAVORITE.Pressed(pressed)) {
        if (sManageCatIndex > 0 && sManageCatIndex <= catCount) {
            const auto& categories = Settings::Get().categories;
            uint16_t catId = categories[sManageCatIndex - 1].id;
            bool currentlyHidden = Settings::IsCategoryHidden(catId);
            Settings::SetCategoryHidden(catId, !currentlyHidden);
        }
    }

    // Move category up (L)
    if (Buttons::Actions::NAV_PAGE_UP.Pressed(pressed)) {
        if (sManageCatIndex > 1 && sManageCatIndex <= catCount) {
            const auto& categories = Settings::Get().categories;
            uint16_t catId = categories[sManageCatIndex - 1].id;
            Settings::MoveCategoryUp(catId);
            // Follow the moved category
            sManageCatIndex--;
        }
    }

    // Move category down (R)
    if (Buttons::Actions::NAV_PAGE_DOWN.Pressed(pressed)) {
        if (sManageCatIndex > 0 && sManageCatIndex < catCount) {
            const auto& categories = Settings::Get().categories;
            uint16_t catId = categories[sManageCatIndex - 1].id;
            Settings::MoveCategoryDown(catId);
            // Follow the moved category
            sManageCatIndex++;
        }
    }

    // Back
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        Settings::Save();
        sSettingsSubMode = SettingsSubMode::MAIN;
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
        case SettingsSubMode::COLOR_INPUT:
            handleColorInputInput(pressed, held);
            break;
        case SettingsSubMode::NAME_INPUT:
            handleNameInputInput(pressed, held);
            break;
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
    sSelectedIndex = Settings::Get().lastIndex;
    clampSelection();

    // Mark as open and reset mode
    sIsOpen = true;
    sCurrentMode = Mode::BROWSE;

    // Run the menu loop (blocks until menu closes)
    uint64_t titleToLaunch = runMenuLoop();

    // Save state
    Settings::Get().lastIndex = sSelectedIndex;
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
