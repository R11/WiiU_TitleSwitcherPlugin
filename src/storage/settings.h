/**
 * Settings Storage System
 *
 * This module handles persistent storage of user preferences using the
 * WUPS (Wii U Plugin System) Storage API. Settings are saved to the SD card
 * and persist across reboots.
 *
 * STORAGE LOCATION:
 * -----------------
 * Settings are stored at: sd:/wiiu/plugins/config/TitleSwitcher.json
 * (Managed automatically by WUPS - don't edit manually while plugin is running)
 *
 * WHAT'S STORED:
 * --------------
 * - configVersion: For detecting and migrating old settings formats
 * - lastIndex: Which title was last selected (for quick resume)
 * - Colors: Background, text, highlight, favorite, header colors
 * - favorites: List of favorited title IDs
 * - categories: User-defined category names
 * - titleCategories: Which titles belong to which categories
 *
 * USAGE:
 * ------
 *   // At plugin startup:
 *   Settings::Init();
 *   Settings::Load();
 *
 *   // Access settings:
 *   uint32_t bgColor = Settings::Get().bgColor;
 *
 *   // Modify settings:
 *   Settings::Get().lastIndex = 42;
 *
 *   // Save to disk:
 *   Settings::Save();
 *
 * THREAD SAFETY:
 * --------------
 * The settings are stored in a global structure. This is fine for a plugin
 * that runs on the main thread, but avoid accessing from callbacks or
 * interrupts without proper synchronization.
 *
 * REFERENCE:
 * ----------
 * - WUPS Storage API: https://github.com/wiiu-env/WiiUPluginSystem/wiki/Storage-API
 */

#pragma once

#include <cstdint>
#include <vector>
#include "../ui/layout.h"

namespace Settings {

// =============================================================================
// Version
// =============================================================================

// Current settings version - increment this when the storage format changes
// Old versions will be detected and migrated (or reset to defaults)
constexpr int32_t CONFIG_VERSION = 2;

// =============================================================================
// Limits
// =============================================================================

// Maximum number of favorites a user can have
constexpr int MAX_FAVORITES = 64;

// Maximum number of custom categories
constexpr int MAX_CATEGORIES = 16;

// Maximum length of a category name (including null terminator)
constexpr int MAX_CATEGORY_NAME = 32;

// Maximum number of title-to-category assignments
// (titles can belong to multiple categories)
constexpr int MAX_TITLE_CATEGORIES = 512;

// =============================================================================
// Default Colors
// =============================================================================
// Colors are in RGBA format: 0xRRGGBBAA
// These defaults use the Catppuccin Mocha color palette.
// Reference: https://catppuccin.com/palette
// =============================================================================

constexpr uint32_t DEFAULT_BG_COLOR          = 0x1E1E2EFF;  // Base
constexpr uint32_t DEFAULT_TITLE_COLOR       = 0xCDD6F4FF;  // Text
constexpr uint32_t DEFAULT_HIGHLIGHTED_COLOR = 0x89B4FAFF;  // Blue
constexpr uint32_t DEFAULT_FAVORITE_COLOR    = 0xF9E2AFFF;  // Yellow
constexpr uint32_t DEFAULT_HEADER_COLOR      = 0xA6E3A1FF;  // Green
constexpr uint32_t DEFAULT_CATEGORY_COLOR    = 0xF5C2E7FF;  // Pink

// =============================================================================
// Data Structures
// =============================================================================

/**
 * A user-defined category.
 *
 * Categories allow users to organize their games beyond just favorites.
 * Examples: "Action", "RPG", "Multiplayer", "Kids", etc.
 */
struct Category {
    // Unique identifier for this category (1-based, 0 means unused)
    // IDs are never reused even after deletion to maintain references
    uint16_t id;

    // Display order (lower = earlier in list, 0 = default)
    uint16_t order;

    // Display name for the category
    char name[MAX_CATEGORY_NAME];

    // Whether this category is hidden from the main menu category bar
    bool hidden;

    // Padding for alignment
    uint8_t _padding[3];

    // Default constructor
    Category() : id(0), order(0), hidden(false) {
        name[0] = '\0';
        _padding[0] = _padding[1] = _padding[2] = 0;
    }
};

/**
 * Assignment of a title to a category.
 *
 * A title can belong to multiple categories. Each assignment is stored
 * as a separate entry in the titleCategories array.
 */
struct TitleCategoryAssignment {
    // The title this assignment is for
    uint64_t titleId;

    // The category ID the title belongs to
    uint16_t categoryId;

    // Padding for alignment
    uint16_t _padding;

    // Default constructor
    TitleCategoryAssignment() : titleId(0), categoryId(0), _padding(0) {}
};

/**
 * Main settings structure.
 *
 * This holds all persistent user preferences. Access it via Settings::Get().
 */
struct PluginSettings {
    // -------------------------------------------------------------------------
    // Metadata
    // -------------------------------------------------------------------------

    // Settings format version (for migrations)
    int32_t configVersion;

    // -------------------------------------------------------------------------
    // Navigation State
    // -------------------------------------------------------------------------

    // Last selected title index (to restore position on menu open)
    int32_t lastIndex;

    // Last selected category index
    int32_t lastCategoryIndex;

    // -------------------------------------------------------------------------
    // Display Options
    // -------------------------------------------------------------------------

    // Show numbers next to titles (e.g., "001. Game Name")
    // Default: false (off)
    bool showNumbers;

    // Show favorite marker (*) next to titles in the list
    // Default: true (on) - can be turned off since favorites category exists
    bool showFavorites;

    // -------------------------------------------------------------------------
    // Layout Preferences
    // -------------------------------------------------------------------------

    // User-customizable layout settings
    Layout::LayoutPreferences layoutPrefs;

    // -------------------------------------------------------------------------
    // Colors
    // -------------------------------------------------------------------------

    uint32_t bgColor;              // Menu background
    uint32_t titleColor;           // Normal title text
    uint32_t highlightedTitleColor;// Selected title text
    uint32_t favoriteColor;        // Favorite marker (*)
    uint32_t headerColor;          // Header text
    uint32_t categoryColor;        // Category tab text

    // -------------------------------------------------------------------------
    // Favorites
    // -------------------------------------------------------------------------

    // List of favorited title IDs
    // Using vector for convenience; stored as binary blob in WUPS storage
    std::vector<uint64_t> favorites;

    // -------------------------------------------------------------------------
    // Categories
    // -------------------------------------------------------------------------

    // User-defined categories
    std::vector<Category> categories;

    // Title-to-category assignments
    std::vector<TitleCategoryAssignment> titleCategories;

    // Next category ID to assign (incremented each time a category is created)
    uint16_t nextCategoryId;

    // -------------------------------------------------------------------------
    // Constructor with defaults
    // -------------------------------------------------------------------------

    PluginSettings() :
        configVersion(CONFIG_VERSION),
        lastIndex(0),
        lastCategoryIndex(0),
        showNumbers(false),
        showFavorites(true),
        layoutPrefs(Layout::LayoutPreferences::Default()),
        bgColor(DEFAULT_BG_COLOR),
        titleColor(DEFAULT_TITLE_COLOR),
        highlightedTitleColor(DEFAULT_HIGHLIGHTED_COLOR),
        favoriteColor(DEFAULT_FAVORITE_COLOR),
        headerColor(DEFAULT_HEADER_COLOR),
        categoryColor(DEFAULT_CATEGORY_COLOR),
        nextCategoryId(1)
    {}
};

// =============================================================================
// Core Functions
// =============================================================================

/**
 * Initialize the settings system.
 *
 * Call this once at plugin startup, before Load().
 * Sets up the global settings structure with default values.
 */
void Init();

/**
 * Load settings from persistent storage.
 *
 * Reads settings from the WUPS storage (SD card).
 * If no saved settings exist, defaults are used.
 * If settings version is old, migration may be performed.
 *
 * Call this once at plugin startup, after Init().
 */
void Load();

/**
 * Save settings to persistent storage.
 *
 * Writes current settings to the WUPS storage (SD card).
 * Call this after modifying settings that should persist.
 *
 * NOTE: Saving is not instantaneous - it writes to SD card.
 *       Avoid calling this every frame; batch changes and save once.
 */
void Save();

/**
 * Get a reference to the settings structure.
 *
 * Use this to read or modify settings:
 *   Settings::Get().bgColor = 0xFF0000FF;  // Set background to red
 *
 * @return Reference to the global PluginSettings structure
 */
PluginSettings& Get();

/**
 * Reset all settings to defaults.
 *
 * This clears all customizations including favorites and categories.
 * Call Save() afterward if you want the reset to persist.
 */
void ResetToDefaults();

// =============================================================================
// Favorites Functions
// =============================================================================

/**
 * Check if a title is favorited.
 *
 * @param titleId The title to check
 * @return true if the title is in the favorites list
 */
bool IsFavorite(uint64_t titleId);

/**
 * Toggle a title's favorite status.
 *
 * If the title is favorited, removes it. If not, adds it.
 * Does NOT automatically save - call Save() when appropriate.
 *
 * @param titleId The title to toggle
 */
void ToggleFavorite(uint64_t titleId);

/**
 * Add a title to favorites.
 *
 * No-op if already favorited or if favorites list is full.
 *
 * @param titleId The title to favorite
 */
void AddFavorite(uint64_t titleId);

/**
 * Remove a title from favorites.
 *
 * No-op if not currently favorited.
 *
 * @param titleId The title to unfavorite
 */
void RemoveFavorite(uint64_t titleId);

// =============================================================================
// Category Functions
// =============================================================================

/**
 * Create a new category.
 *
 * @param name Display name for the category (will be truncated if too long)
 * @return ID of the new category, or 0 if creation failed (list full)
 */
uint16_t CreateCategory(const char* name);

/**
 * Delete a category.
 *
 * This also removes all title assignments for this category.
 *
 * @param categoryId ID of the category to delete
 */
void DeleteCategory(uint16_t categoryId);

/**
 * Rename a category.
 *
 * @param categoryId ID of the category to rename
 * @param newName New display name
 */
void RenameCategory(uint16_t categoryId, const char* newName);

/**
 * Get a category by ID.
 *
 * @param categoryId ID of the category to find
 * @return Pointer to the category, or nullptr if not found
 */
const Category* GetCategory(uint16_t categoryId);

/**
 * Get the number of user-defined categories.
 *
 * @return Number of categories (not including built-in All/Favorites)
 */
int GetCategoryCount();

/**
 * Check if a title belongs to a category.
 *
 * @param titleId The title to check
 * @param categoryId The category to check
 * @return true if the title is assigned to the category
 */
bool TitleHasCategory(uint64_t titleId, uint16_t categoryId);

/**
 * Assign a title to a category.
 *
 * A title can belong to multiple categories.
 * No-op if already assigned or if assignment list is full.
 *
 * @param titleId The title to assign
 * @param categoryId The category to assign to
 */
void AssignTitleToCategory(uint64_t titleId, uint16_t categoryId);

/**
 * Remove a title from a category.
 *
 * @param titleId The title to unassign
 * @param categoryId The category to remove from
 */
void RemoveTitleFromCategory(uint64_t titleId, uint16_t categoryId);

/**
 * Get all category IDs that a title belongs to.
 *
 * @param titleId The title to query
 * @param outIds Array to receive category IDs
 * @param maxIds Maximum number of IDs to return
 * @return Number of categories the title belongs to
 */
int GetCategoriesForTitle(uint64_t titleId, uint16_t* outIds, int maxIds);

// =============================================================================
// Category Visibility and Ordering
// =============================================================================

/**
 * Set whether a category is hidden from the main menu.
 *
 * @param categoryId ID of the category
 * @param hidden true to hide, false to show
 */
void SetCategoryHidden(uint16_t categoryId, bool hidden);

/**
 * Check if a category is hidden.
 *
 * @param categoryId ID of the category
 * @return true if hidden, false if visible
 */
bool IsCategoryHidden(uint16_t categoryId);

/**
 * Move a category up in the display order.
 *
 * @param categoryId ID of the category to move
 */
void MoveCategoryUp(uint16_t categoryId);

/**
 * Move a category down in the display order.
 *
 * @param categoryId ID of the category to move
 */
void MoveCategoryDown(uint16_t categoryId);

/**
 * Get categories sorted by display order.
 * Returns indices into the categories vector.
 *
 * @param outIndices Array to receive sorted indices
 * @param maxCount Maximum number of indices to return
 * @param includeHidden If true, include hidden categories
 * @return Number of categories returned
 */
int GetSortedCategoryIndices(int* outIndices, int maxCount, bool includeHidden = true);

} // namespace Settings
