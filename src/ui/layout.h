/**
 * Declarative Layout System
 *
 * Defines screen layouts as data, enabling:
 * - Easy reorganization of UI elements
 * - Different layouts per screen type (DRC, TV resolutions)
 * - Future user customization via settings
 *
 * USAGE:
 * ------
 *   // Get layout for current screen
 *   const ScreenLayout& layout = Layout::GetLayout(ScreenType::DRC, MenuScreen::BROWSE);
 *
 *   // Check if section is visible and get bounds
 *   if (layout.IsVisible(Section::DETAILS_ICON)) {
 *       auto bounds = layout.Get(Section::DETAILS_ICON);
 *       drawIcon(bounds.col, bounds.row, bounds.width, bounds.height);
 *   }
 */

#pragma once

#include <cstdint>

namespace Layout {

// =============================================================================
// Screen Types
// =============================================================================

/**
 * Physical screen types with different resolutions.
 */
enum class ScreenType {
    DRC,        // GamePad: 854x480, ~100 cols x 18 rows
    TV_1080P,   // TV 1080p: 1920x1080
    TV_720P,    // TV 720p: 1280x720
    TV_480P,    // TV 480p: 720x480

    COUNT
};

// =============================================================================
// Menu Screens
// =============================================================================

/**
 * Different menu screens/modes that have their own layouts.
 */
enum class MenuScreen {
    BROWSE,             // Main title browsing
    EDIT,               // Edit title categories
    SETTINGS_MAIN,      // Main settings list
    SETTINGS_CATEGORIES,// Category management
    SETTINGS_SYSTEM_APPS,// System apps list
    TEXT_INPUT,         // Text/color input

    COUNT
};

// =============================================================================
// Sections
// =============================================================================

/**
 * Individual UI sections that can be positioned independently.
 * Granular sections allow fine control over layout.
 */
enum class Section {
    // Common sections
    CATEGORY_BAR,       // Top category tabs
    HEADER,             // Header/divider line
    FOOTER,             // Bottom status/controls

    // Left panel sections
    TITLE_LIST,         // Main title list (browse mode)
    SETTINGS_LIST,      // Settings items list
    CATEGORY_LIST,      // Category management list
    SYSTEM_APPS_LIST,   // System apps list

    // Right panel - details breakdown
    DETAILS_TITLE,      // Title name at top of details
    DETAILS_ICON,       // Game icon
    DETAILS_BASIC_INFO, // ID, favorite, game ID
    DETAILS_PRESET,     // Publisher, developer, date, genre
    DETAILS_CATEGORIES, // Category assignments

    // Right panel - other screens
    SETTINGS_DESC,      // Settings item description
    CATEGORY_DETAILS,   // Category details/actions
    SYSTEM_APP_DESC,    // System app description

    // Edit mode sections
    EDIT_TITLE_INFO,    // Title being edited (left side)
    EDIT_CATEGORIES,    // Category checkboxes (right side)

    // Input sections
    INPUT_PROMPT,       // Input prompt text
    INPUT_FIELD,        // Text input field
    INPUT_HINTS,        // Input control hints

    COUNT
};

// =============================================================================
// Section Bounds
// =============================================================================

/**
 * Render layers for controlling draw order.
 * Lower values render first (underneath), higher values render on top.
 */
enum class Layer : int {
    BACKGROUND = 0,     // Background elements
    CONTENT = 100,      // Main content (lists, panels)
    OVERLAY = 200,      // Overlays (popups, dialogs)
    TOP = 300,          // Always on top (tooltips, notifications)
};

/**
 * Position and size of a section in grid coordinates.
 */
struct SectionBounds {
    int col;            // Starting column
    int row;            // Starting row
    int width;          // Width in columns
    int height;         // Height in rows
    bool visible;       // Whether section is shown
    int zIndex;         // Render order (higher = on top, default 100)

    // Convenience: check if bounds are valid
    bool IsValid() const { return visible && width > 0 && height > 0; }
};

// =============================================================================
// Screen Layout
// =============================================================================

/**
 * Entry for iterating sections in render order.
 */
struct SectionEntry {
    Section section;
    const SectionBounds* bounds;
};

/**
 * Complete layout definition for one screen type + menu screen combination.
 */
class ScreenLayout {
public:
    /**
     * Get bounds for a section.
     * Returns zero-sized invisible bounds if section not defined.
     */
    const SectionBounds& Get(Section section) const;

    /**
     * Check if a section is visible in this layout.
     */
    bool IsVisible(Section section) const;

    /**
     * Set bounds for a section (used during layout construction).
     */
    void Set(Section section, const SectionBounds& bounds);

    /**
     * Get all visible sections sorted by zIndex (render order).
     * Lower zIndex values come first (render underneath).
     *
     * @param outEntries  Array to fill with section entries
     * @param maxEntries  Maximum entries to return
     * @return Number of entries written
     */
    int GetSectionsInRenderOrder(SectionEntry* outEntries, int maxEntries) const;

    /**
     * Get grid dimensions for this layout.
     */
    int GetGridWidth() const { return gridWidth; }
    int GetGridHeight() const { return gridHeight; }

    /**
     * Set grid dimensions (used during layout construction).
     */
    void SetGridSize(int width, int height) {
        gridWidth = width;
        gridHeight = height;
    }

private:
    SectionBounds sections[static_cast<int>(Section::COUNT)] = {};
    int gridWidth = 100;
    int gridHeight = 18;
};

// =============================================================================
// Layout Access
// =============================================================================

/**
 * Get the layout for a specific screen type and menu screen.
 */
const ScreenLayout& GetLayout(ScreenType screen, MenuScreen menu);

/**
 * Get the current screen type (based on active display).
 * For now, returns DRC since OSScreen renders to both identically.
 */
ScreenType GetCurrentScreenType();

/**
 * Get grid dimensions for a screen type.
 */
void GetGridDimensions(ScreenType screen, int& outWidth, int& outHeight);

// =============================================================================
// Layout Initialization
// =============================================================================

/**
 * Initialize all layout tables.
 * Called once at startup.
 */
void Init();

} // namespace Layout
