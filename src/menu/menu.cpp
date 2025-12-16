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
#include "../editor/pixel_editor.h"

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

constexpr int SETTINGS_ITEM_COUNT = 9;  // Number of settings items

int sSettingsIndex = 0;              // Currently selected setting
SettingsSubMode sSettingsSubMode = SettingsSubMode::MAIN;
int sManageCatIndex = 0;             // Selected category in manage mode
int sManageCatScroll = 0;            // Scroll offset in manage mode
int sEditingColorIndex = -1;         // Which color is being edited (-1 = none)
int sEditingCategoryId = -1;         // Which category is being renamed (-1 = new)
TextInput::Field sInputField;        // Reusable text input field

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
    if (sSelectedIndex >= sScrollOffset + VISIBLE_ROWS) {
        sScrollOffset = sSelectedIndex - VISIBLE_ROWS + 1;
    }

    // Clamp scroll offset
    if (sScrollOffset < 0) {
        sScrollOffset = 0;
    }
    int maxScroll = count - VISIBLE_ROWS;
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

    // Draw each category tab
    int catCount = Categories::GetTotalCategoryCount();
    int currentCat = Categories::GetCurrentCategoryIndex();

    for (int i = 0; i < catCount && col < 60; i++) {
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
    for (int row = LIST_START_ROW; row < FOOTER_ROW; row++) {
        Renderer::DrawText(DIVIDER_COL, row, "|");
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
    int maxNameLen = showNums ? TITLE_NAME_WIDTH_NUM : TITLE_NAME_WIDTH;

    // Draw visible titles
    for (int i = 0; i < VISIBLE_ROWS && (sScrollOffset + i) < count; i++) {
        int idx = sScrollOffset + i;
        const Titles::TitleInfo* title = Categories::GetFilteredTitle(idx);

        if (!title) continue;

        char line[48];

        // Check if this title is a favorite
        bool isFav = Settings::IsFavorite(title->titleId);

        // Truncate name to fit
        char displayName[32];
        strncpy(displayName, title->name, maxNameLen);
        displayName[maxNameLen] = '\0';

        // Format depends on showNumbers setting
        const char* favMark = isFav ? "*" : " ";
        const char* cursor = (idx == sSelectedIndex) ? ">" : " ";

        if (showNums) {
            // Format: "> 001. * Name"
            snprintf(line, sizeof(line), "%s%3d. %s %-*s",
                     cursor, idx + 1, favMark, maxNameLen, displayName);
        } else {
            // Format: "> * Name" (no numbers)
            snprintf(line, sizeof(line), "%s %s %-*s",
                     cursor, favMark, maxNameLen, displayName);
        }

        Renderer::DrawText(LIST_START_COL, LIST_START_ROW + i, line);
    }

    // Scroll indicators
    if (sScrollOffset > 0) {
        Renderer::DrawText(LIST_WIDTH - 4, LIST_START_ROW, "[UP]");
    }
    if (sScrollOffset + VISIBLE_ROWS < count) {
        Renderer::DrawText(LIST_WIDTH - 6, LIST_START_ROW + VISIBLE_ROWS - 1, "[DOWN]");
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
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW, title->name);

    // Draw icon below title (128x128 = 2x the original 64x64)
    constexpr int ICON_SIZE = 128;
    int iconX = Renderer::ColToPixelX(DETAILS_START_COL);
    int iconY = Renderer::RowToPixelY(LIST_START_ROW + 1);  // Below title

    if (ImageLoader::IsReady(title->titleId)) {
        Renderer::ImageHandle icon = ImageLoader::Get(title->titleId);
        Renderer::DrawImage(iconX, iconY, icon, ICON_SIZE, ICON_SIZE);
    } else {
        // Draw placeholder while loading
        Renderer::DrawPlaceholder(iconX, iconY, ICON_SIZE, ICON_SIZE, 0x333333FF);
    }

    // Info starts after icon (icon is ~5-6 rows at 24px/row)
    constexpr int INFO_START_ROW = LIST_START_ROW + 7;

    // Title ID
    char idStr[32];
    snprintf(idStr, sizeof(idStr), "ID: %016llX",
             static_cast<unsigned long long>(title->titleId));
    Renderer::DrawText(DETAILS_START_COL, INFO_START_ROW, idStr);

    // Favorite status
    const char* favStatus = Settings::IsFavorite(title->titleId) ? "Yes" : "No";
    Renderer::DrawTextF(DETAILS_START_COL, INFO_START_ROW + 1, "Favorite: %s", favStatus);

    // Categories this title belongs to
    Renderer::DrawText(DETAILS_START_COL, INFO_START_ROW + 3, "Categories:");

    uint16_t catIds[Settings::MAX_CATEGORIES];
    int catCount = Settings::GetCategoriesForTitle(title->titleId, catIds, Settings::MAX_CATEGORIES);

    if (catCount == 0) {
        Renderer::DrawText(DETAILS_START_COL + 2, INFO_START_ROW + 4, "(none)");
    } else {
        int row = INFO_START_ROW + 4;
        for (int i = 0; i < catCount && row < FOOTER_ROW - 1; i++) {
            const Settings::Category* cat = Settings::GetCategory(catIds[i]);
            if (cat) {
                Renderer::DrawTextF(DETAILS_START_COL + 2, row, "- %s", cat->name);
                row++;
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

    Renderer::DrawText(0, FOOTER_ROW, footer);
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
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW, "Categories:");
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 1, "------------");

    // Get user-defined categories
    int catCount = Settings::GetCategoryCount();
    const auto& categories = Settings::Get().categories;

    if (catCount == 0) {
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 3,
                         "No categories defined.");
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 4,
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

            Renderer::DrawTextF(DETAILS_START_COL, startRow + i,
                              "%s %s %s", cursor, checkbox, cat.name);
        }

        // Scroll indicators
        if (sEditCategoryScroll > 0) {
            Renderer::DrawText(DETAILS_START_COL + 20, startRow, "[UP]");
        }
        if (sEditCategoryScroll + CAT_VISIBLE_ROWS < catCount) {
            Renderer::DrawText(DETAILS_START_COL + 18, startRow + CAT_VISIBLE_ROWS - 1, "[DOWN]");
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
    Renderer::DrawText(0, FOOTER_ROW, footer);
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
 * Helper: Get color pointer by index for settings editing.
 */
uint32_t* getColorByIndex(int index)
{
    auto& settings = Settings::Get();
    switch (index) {
        case 1: return &settings.bgColor;
        case 2: return &settings.titleColor;
        case 3: return &settings.highlightedTitleColor;
        case 4: return &settings.favoriteColor;
        case 5: return &settings.headerColor;
        case 6: return &settings.categoryColor;
        default: return nullptr;
    }
}

/**
 * Helper: Get color name by index.
 */
const char* getColorName(int index)
{
    switch (index) {
        case 1: return "Background";
        case 2: return "Title Text";
        case 3: return "Highlighted";
        case 4: return "Favorite";
        case 5: return "Header";
        case 6: return "Category";
        default: return "Unknown";
    }
}

/**
 * Helper: Get setting description by index.
 */
const char* getSettingDescription(int index)
{
    switch (index) {
        case 0: return "Show line numbers before\neach title in the list.";
        case 1: return "Menu background color.\nRGBA hex format.";
        case 2: return "Normal title text color.\nRGBA hex format.";
        case 3: return "Selected title color.\nRGBA hex format.";
        case 4: return "Favorite marker color.\nRGBA hex format.";
        case 5: return "Header text color.\nRGBA hex format.";
        case 6: return "Category tab color.\nRGBA hex format.";
        case 7: return "Create, rename, or delete\ncustom categories.";
        case 8: return "Open the pixel editor\nto create custom graphics.";
        default: return "";
    }
}

/**
 * Render the main settings list.
 */
void renderSettingsMain()
{
    auto& settings = Settings::Get();

    // --- Header ---
    Renderer::DrawText(0, 0, "SETTINGS");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    // --- Divider ---
    drawDivider();

    // --- Left side: Settings list ---
    const char* cursor;
    int row = LIST_START_ROW;

    // 0: Show Numbers
    cursor = (sSettingsIndex == 0) ? ">" : " ";
    Renderer::DrawTextF(0, row++, "%s Show Numbers: %s", cursor,
                      settings.showNumbers ? "ON" : "OFF");

    // 1-6: Colors
    for (int i = 1; i <= 6; i++) {
        cursor = (sSettingsIndex == i) ? ">" : " ";
        uint32_t* color = getColorByIndex(i);
        Renderer::DrawTextF(0, row++, "%s %s: %08X", cursor,
                          getColorName(i), *color);
    }

    // 7: Manage Categories
    cursor = (sSettingsIndex == 7) ? ">" : " ";
    Renderer::DrawTextF(0, row++, "%s Manage Categories (%d)",
                      cursor, Settings::GetCategoryCount());

    // 8: Pixel Editor
    cursor = (sSettingsIndex == 8) ? ">" : " ";
    Renderer::DrawTextF(0, row++, "%s Pixel Editor", cursor);

    // --- Right side: Description ---
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW, "Description:");
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 1, "------------");

    // Show description for selected item
    const char* desc = getSettingDescription(sSettingsIndex);

    // Simple line splitting for description
    char line1[32] = {0};
    char line2[32] = {0};
    const char* newline = strchr(desc, '\n');
    if (newline) {
        int len1 = newline - desc;
        if (len1 > 30) len1 = 30;
        strncpy(line1, desc, len1);
        strncpy(line2, newline + 1, 30);
    } else {
        strncpy(line1, desc, 30);
    }

    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 3, line1);
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 4, line2);

    // Show current value preview for colors
    if (sSettingsIndex >= 1 && sSettingsIndex <= 6) {
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 6, "Press A to edit");
    } else if (sSettingsIndex == 0) {
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 6, "Press A to toggle");
    } else if (sSettingsIndex == 7 || sSettingsIndex == 8) {
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 6, "Press A to open");
    }

    // --- Footer ---
    Renderer::DrawTextF(0, FOOTER_ROW, "%s:Edit %s:Back  [%d/%d]",
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
            Renderer::DrawTextF(0, row + i, "%s %s", cursor, cat.name);
        }
    }

    // --- Right side: Actions ---
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW, "Actions:");
    Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 1, "--------");

    if (sManageCatIndex == 0) {
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 3, "Create a new category");
        Renderer::DrawText(DETAILS_START_COL, LIST_START_ROW + 4, "for organizing titles.");
        Renderer::DrawTextF(DETAILS_START_COL, LIST_START_ROW + 6, "%s: Create",
                          Buttons::Actions::CONFIRM.label);
    } else if (sManageCatIndex <= catCount) {
        const Settings::Category& cat = categories[sManageCatIndex - 1];
        Renderer::DrawTextF(DETAILS_START_COL, LIST_START_ROW + 3, "Category: %s", cat.name);
        Renderer::DrawTextF(DETAILS_START_COL, LIST_START_ROW + 5, "%s: Rename",
                          Buttons::Actions::CONFIRM.label);
        Renderer::DrawTextF(DETAILS_START_COL, LIST_START_ROW + 6, "%s: Delete",
                          Buttons::Actions::EDIT.label);
    }

    // --- Footer ---
    Renderer::DrawTextF(0, FOOTER_ROW, "%s:Select %s:Back  [%d/%d]",
                      Buttons::Actions::CONFIRM.label,
                      Buttons::Actions::CANCEL.label,
                      sManageCatIndex + 1, catCount + 1);
}

/**
 * Render color input mode.
 */
void renderColorInput()
{
    Renderer::DrawText(0, 0, "EDIT COLOR");
    Renderer::DrawText(0, HEADER_ROW, "------------------------------------------------------------");

    Renderer::DrawTextF(0, LIST_START_ROW, "Editing: %s Color", getColorName(sEditingColorIndex));
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

    // Confirm/Edit
    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (sSettingsIndex == 0) {
            // Toggle showNumbers
            Settings::Get().showNumbers = !Settings::Get().showNumbers;
        } else if (sSettingsIndex >= 1 && sSettingsIndex <= 6) {
            // Edit color - initialize input field with current value
            sEditingColorIndex = sSettingsIndex;
            uint32_t* color = getColorByIndex(sSettingsIndex);
            char hexStr[16];
            snprintf(hexStr, sizeof(hexStr), "%08X", *color);
            sInputField.Init(8, TextInput::Library::HEX);
            sInputField.SetValue(hexStr);
            sSettingsSubMode = SettingsSubMode::COLOR_INPUT;
        } else if (sSettingsIndex == 7) {
            // Open manage categories
            sManageCatIndex = 0;
            sManageCatScroll = 0;
            sSettingsSubMode = SettingsSubMode::MANAGE_CATS;
        } else if (sSettingsIndex == 8) {
            // Open pixel editor
            PixelEditor::Config editorConfig;
            editorConfig.width = 64;
            editorConfig.height = 64;
            editorConfig.savePath = "fs:/vol/external01/wiiu/titleswitcher/";
            PixelEditor::Open(editorConfig);
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

        // Set the color
        uint32_t* color = getColorByIndex(sEditingColorIndex);
        if (color) {
            *color = value;
        }

        sEditingColorIndex = -1;
        sSettingsSubMode = SettingsSubMode::MAIN;
    } else if (result == TextInput::Result::CANCELLED) {
        sEditingColorIndex = -1;
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
