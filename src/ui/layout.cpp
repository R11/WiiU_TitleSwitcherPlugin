/**
 * Declarative Layout System - Implementation
 */

#include "layout.h"

namespace Layout {

// =============================================================================
// ScreenLayout Implementation
// =============================================================================

namespace {
    // Default empty bounds for undefined sections
    const SectionBounds EMPTY_BOUNDS = { 0, 0, 0, 0, false, 0 };
}

const SectionBounds& ScreenLayout::Get(Section section) const {
    int idx = static_cast<int>(section);
    if (idx >= 0 && idx < static_cast<int>(Section::COUNT)) {
        return sections[idx];
    }
    return EMPTY_BOUNDS;
}

bool ScreenLayout::IsVisible(Section section) const {
    return Get(section).visible;
}

void ScreenLayout::Set(Section section, const SectionBounds& bounds) {
    int idx = static_cast<int>(section);
    if (idx >= 0 && idx < static_cast<int>(Section::COUNT)) {
        sections[idx] = bounds;
    }
}

int ScreenLayout::GetSectionsInRenderOrder(SectionEntry* outEntries, int maxEntries) const {
    if (!outEntries || maxEntries <= 0) return 0;

    // Collect visible sections
    int count = 0;
    for (int i = 0; i < static_cast<int>(Section::COUNT) && count < maxEntries; i++) {
        if (sections[i].visible) {
            outEntries[count].section = static_cast<Section>(i);
            outEntries[count].bounds = &sections[i];
            count++;
        }
    }

    // Simple insertion sort by zIndex (small list, no need for complex sort)
    for (int i = 1; i < count; i++) {
        SectionEntry temp = outEntries[i];
        int j = i - 1;
        while (j >= 0 && outEntries[j].bounds->zIndex > temp.bounds->zIndex) {
            outEntries[j + 1] = outEntries[j];
            j--;
        }
        outEntries[j + 1] = temp;
    }

    return count;
}

// =============================================================================
// Layout Storage
// =============================================================================

namespace {

// Layout tables: [ScreenType][MenuScreen]
ScreenLayout sLayouts[static_cast<int>(ScreenType::COUNT)]
                     [static_cast<int>(MenuScreen::COUNT)];

bool sInitialized = false;

// =============================================================================
// Grid Dimensions Per Screen Type
// =============================================================================

struct GridSize {
    int width;
    int height;
};

const GridSize GRID_SIZES[static_cast<int>(ScreenType::COUNT)] = {
    { 100, 18 },  // DRC: ~100 cols x 18 rows
    { 120, 24 },  // TV_1080P: larger grid
    { 110, 20 },  // TV_720P: medium grid
    {  90, 16 },  // TV_480P: smaller grid
};

// =============================================================================
// Layout Builders
// =============================================================================

// Layer constants for zIndex
constexpr int Z_BACKGROUND = static_cast<int>(Layer::BACKGROUND);
constexpr int Z_CONTENT = static_cast<int>(Layer::CONTENT);
constexpr int Z_OVERLAY = static_cast<int>(Layer::OVERLAY);
constexpr int Z_TOP = static_cast<int>(Layer::TOP);

void BuildBrowseLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int dividerCol = (grid.width * 30) / 100;  // 30% for title list
    int detailsCol = dividerCol + 2;
    int detailsWidth = grid.width - detailsCol - 1;
    int contentHeight = grid.height - 3;  // Minus category bar, header, footer
    int footerRow = grid.height - 1;

    // Common sections (chrome renders on top of content)
    layout.Set(Section::CATEGORY_BAR, { 0, 0, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // Title list (left side)
    layout.Set(Section::TITLE_LIST, { 0, 2, dividerCol, contentHeight, true, Z_CONTENT });

    // Details panel sections (right side)
    // Layout varies by screen size
    if (screen == ScreenType::TV_480P) {
        // Small screen: hide some details
        layout.Set(Section::DETAILS_TITLE, { detailsCol, 2, detailsWidth, 1, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_ICON, { 0, 0, 0, 0, false, 0 });  // Hidden
        layout.Set(Section::DETAILS_BASIC_INFO, { detailsCol, 3, detailsWidth, 3, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_PRESET, { detailsCol, 7, detailsWidth, 4, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_CATEGORIES, { detailsCol, 12, detailsWidth, 3, true, Z_CONTENT + 10 });
    } else {
        // Normal/large screens: full details
        layout.Set(Section::DETAILS_TITLE, { detailsCol, 2, detailsWidth, 1, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_ICON, { detailsCol, 3, detailsWidth, 5, true, Z_CONTENT + 5 });  // Icon under text
        layout.Set(Section::DETAILS_BASIC_INFO, { detailsCol, 9, detailsWidth, 3, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_PRESET, { detailsCol, 13, detailsWidth, 4, true, Z_CONTENT + 10 });
        layout.Set(Section::DETAILS_CATEGORIES, { detailsCol, 18, detailsWidth, 4,
            screen != ScreenType::DRC, Z_CONTENT + 10 });  // Only on larger screens
    }
}

void BuildEditLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int dividerCol = (grid.width * 30) / 100;
    int detailsCol = dividerCol + 2;
    int detailsWidth = grid.width - detailsCol - 1;
    int footerRow = grid.height - 1;

    // Chrome (renders on top)
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // Left side: title being edited
    layout.Set(Section::EDIT_TITLE_INFO, { 0, 2, dividerCol, 4, true, Z_CONTENT });

    // Right side: category checkboxes
    layout.Set(Section::EDIT_CATEGORIES, { detailsCol, 2, detailsWidth, grid.height - 4, true, Z_CONTENT });
}

void BuildSettingsMainLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int dividerCol = (grid.width * 30) / 100;
    int detailsCol = dividerCol + 2;
    int detailsWidth = grid.width - detailsCol - 1;
    int footerRow = grid.height - 1;

    // Chrome (renders on top)
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // Settings list (left)
    layout.Set(Section::SETTINGS_LIST, { 0, 2, dividerCol, grid.height - 4, true, Z_CONTENT });

    // Description (right)
    layout.Set(Section::SETTINGS_DESC, { detailsCol, 2, detailsWidth, grid.height - 4, true, Z_CONTENT + 10 });
}

void BuildCategoryManageLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int dividerCol = (grid.width * 30) / 100;
    int detailsCol = dividerCol + 2;
    int detailsWidth = grid.width - detailsCol - 1;
    int footerRow = grid.height - 1;

    // Chrome (renders on top)
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // Category list (left)
    layout.Set(Section::CATEGORY_LIST, { 0, 2, dividerCol, grid.height - 4, true, Z_CONTENT });

    // Category details/actions (right)
    layout.Set(Section::CATEGORY_DETAILS, { detailsCol, 2, detailsWidth, grid.height - 4, true, Z_CONTENT + 10 });
}

void BuildSystemAppsLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int dividerCol = (grid.width * 30) / 100;
    int detailsCol = dividerCol + 2;
    int detailsWidth = grid.width - detailsCol - 1;
    int footerRow = grid.height - 1;

    // Chrome (renders on top)
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // System apps list (left)
    layout.Set(Section::SYSTEM_APPS_LIST, { 0, 2, dividerCol, grid.height - 4, true, Z_CONTENT });

    // Description (right)
    layout.Set(Section::SYSTEM_APP_DESC, { detailsCol, 2, detailsWidth, grid.height - 4, true, Z_CONTENT + 10 });
}

void BuildTextInputLayout(ScreenLayout& layout, ScreenType screen) {
    const GridSize& grid = GRID_SIZES[static_cast<int>(screen)];
    layout.SetGridSize(grid.width, grid.height);

    int footerRow = grid.height - 1;

    // Chrome (renders on top)
    layout.Set(Section::HEADER, { 0, 1, grid.width, 1, true, Z_CONTENT + 50 });
    layout.Set(Section::FOOTER, { 0, footerRow, grid.width, 1, true, Z_CONTENT + 50 });

    // Full-width input sections
    layout.Set(Section::INPUT_PROMPT, { 0, 3, grid.width, 1, true, Z_CONTENT });
    layout.Set(Section::INPUT_FIELD, { 0, 5, grid.width, 1, true, Z_CONTENT + 10 });
    layout.Set(Section::INPUT_HINTS, { 0, 8, grid.width, 4, true, Z_CONTENT });
}

void BuildAllLayouts() {
    // Build layouts for each screen type and menu screen
    for (int s = 0; s < static_cast<int>(ScreenType::COUNT); s++) {
        ScreenType screen = static_cast<ScreenType>(s);

        BuildBrowseLayout(sLayouts[s][static_cast<int>(MenuScreen::BROWSE)], screen);
        BuildEditLayout(sLayouts[s][static_cast<int>(MenuScreen::EDIT)], screen);
        BuildSettingsMainLayout(sLayouts[s][static_cast<int>(MenuScreen::SETTINGS_MAIN)], screen);
        BuildCategoryManageLayout(sLayouts[s][static_cast<int>(MenuScreen::SETTINGS_CATEGORIES)], screen);
        BuildSystemAppsLayout(sLayouts[s][static_cast<int>(MenuScreen::SETTINGS_SYSTEM_APPS)], screen);
        BuildTextInputLayout(sLayouts[s][static_cast<int>(MenuScreen::TEXT_INPUT)], screen);
    }
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

const ScreenLayout& GetLayout(ScreenType screen, MenuScreen menu) {
    if (!sInitialized) {
        Init();
    }

    int s = static_cast<int>(screen);
    int m = static_cast<int>(menu);

    if (s >= 0 && s < static_cast<int>(ScreenType::COUNT) &&
        m >= 0 && m < static_cast<int>(MenuScreen::COUNT)) {
        return sLayouts[s][m];
    }

    // Return DRC/BROWSE as fallback
    return sLayouts[0][0];
}

ScreenType GetCurrentScreenType() {
    // For now, OSScreen renders identically to both DRC and TV
    // In the future, could detect actual output and return appropriate type
    return ScreenType::DRC;
}

void GetGridDimensions(ScreenType screen, int& outWidth, int& outHeight) {
    int idx = static_cast<int>(screen);
    if (idx >= 0 && idx < static_cast<int>(ScreenType::COUNT)) {
        outWidth = GRID_SIZES[idx].width;
        outHeight = GRID_SIZES[idx].height;
    } else {
        outWidth = 100;
        outHeight = 18;
    }
}

void Init() {
    if (sInitialized) return;

    BuildAllLayouts();
    sInitialized = true;
}

} // namespace Layout
