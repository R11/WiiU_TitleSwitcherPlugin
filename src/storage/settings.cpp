/**
 * Settings Storage Implementation
 *
 * See settings.h for usage documentation.
 *
 * WUPS STORAGE API OVERVIEW:
 * --------------------------
 * The WUPS Storage API provides persistent key-value storage for plugins.
 * Data is stored in a JSON file on the SD card, managed by the WUPS loader.
 *
 * Key functions used:
 *   WUPSStorageAPI_GetInt()    - Read a 32-bit integer
 *   WUPSStorageAPI_StoreInt()  - Write a 32-bit integer
 *   WUPSStorageAPI_GetBinary() - Read binary data (arrays, structs)
 *   WUPSStorageAPI_StoreBinary() - Write binary data
 *   WUPSStorageAPI_SaveStorage() - Flush changes to SD card
 *
 * The first parameter to all functions is a "parent" key for nested storage;
 * we use nullptr for top-level storage.
 *
 * BINARY DATA FORMAT:
 * -------------------
 * For favorites: Array of uint64_t title IDs
 * For categories: Array of Category structs
 * For titleCategories: Array of TitleCategoryAssignment structs
 *
 * Reference: https://github.com/wiiu-env/WiiUPluginSystem/wiki/Storage-API
 */

#include "settings.h"

// WUPS Storage API
#include <wups/storage.h>

// Standard library
#include <cstring>
#include <algorithm>

namespace Settings {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// The global settings instance
PluginSettings gSettings;

// Storage keys (used to identify data in the WUPS storage file)
// These are arbitrary strings; just need to be consistent and unique
constexpr const char* KEY_VERSION          = "configVersion";
constexpr const char* KEY_LAST_INDEX       = "lastIndex";
constexpr const char* KEY_LAST_CATEGORY    = "lastCategory";
constexpr const char* KEY_SHOW_NUMBERS     = "showNumbers";
constexpr const char* KEY_SHOW_FAVORITES   = "showFavorites";
constexpr const char* KEY_BG_COLOR         = "bgColor";
constexpr const char* KEY_TITLE_COLOR      = "titleColor";
constexpr const char* KEY_HIGHLIGHTED      = "highlightedColor";
constexpr const char* KEY_FAVORITE_COLOR   = "favoriteColor";
constexpr const char* KEY_HEADER_COLOR     = "headerColor";
constexpr const char* KEY_CATEGORY_COLOR   = "categoryColor";
constexpr const char* KEY_FAVORITES_COUNT  = "favoritesCount";
constexpr const char* KEY_FAVORITES_DATA   = "favoritesData";
constexpr const char* KEY_CATEGORIES_COUNT = "categoriesCount";
constexpr const char* KEY_CATEGORIES_DATA  = "categoriesData";
constexpr const char* KEY_TITLE_CAT_COUNT  = "titleCatCount";
constexpr const char* KEY_TITLE_CAT_DATA   = "titleCatData";
constexpr const char* KEY_NEXT_CAT_ID      = "nextCategoryId";
constexpr const char* KEY_LAYOUT_FONT_SCALE    = "layoutFontScale";
constexpr const char* KEY_LAYOUT_LIST_WIDTH    = "layoutListWidth";
constexpr const char* KEY_LAYOUT_ICON_SIZE     = "layoutIconSize";

// =============================================================================
// Storage Helpers
// =============================================================================

// Load boolean from storage (stored as int: 0=false, non-zero=true)
inline void LoadBool(const char* key, bool& out) {
    int32_t temp;
    if (WUPSStorageAPI_GetInt(nullptr, key, &temp) == WUPS_STORAGE_ERROR_SUCCESS) {
        out = (temp != 0);
    }
}

// Load color from storage (stored as signed int32, cast to uint32)
inline void LoadColor(const char* key, uint32_t& out) {
    int32_t temp;
    if (WUPSStorageAPI_GetInt(nullptr, key, &temp) == WUPS_STORAGE_ERROR_SUCCESS) {
        out = static_cast<uint32_t>(temp);
    }
}

} // anonymous namespace

// =============================================================================
// Core Functions Implementation
// =============================================================================

void Init()
{
    // Initialize with default values
    gSettings = PluginSettings();
}

void Load()
{
    // -------------------------------------------------------------------------
    // Step 1: Check for existing settings and their version
    // -------------------------------------------------------------------------
    int32_t version = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_VERSION, &version);

    if (version == 0) {
        // No saved settings found - keep defaults
        return;
    }

    // TODO: Handle version migration if version < CONFIG_VERSION
    // For now, we just load what we can

    // -------------------------------------------------------------------------
    // Step 2: Load integer settings
    // -------------------------------------------------------------------------
    WUPSStorageAPI_GetInt(nullptr, KEY_LAST_INDEX, &gSettings.lastIndex);
    WUPSStorageAPI_GetInt(nullptr, KEY_LAST_CATEGORY, &gSettings.lastCategoryIndex);

    // -------------------------------------------------------------------------
    // Step 3: Load boolean settings
    // -------------------------------------------------------------------------
    LoadBool(KEY_SHOW_NUMBERS, gSettings.showNumbers);
    LoadBool(KEY_SHOW_FAVORITES, gSettings.showFavorites);

    // -------------------------------------------------------------------------
    // Step 4: Load colors
    // -------------------------------------------------------------------------
    LoadColor(KEY_BG_COLOR, gSettings.bgColor);
    LoadColor(KEY_TITLE_COLOR, gSettings.titleColor);
    LoadColor(KEY_HIGHLIGHTED, gSettings.highlightedTitleColor);
    LoadColor(KEY_FAVORITE_COLOR, gSettings.favoriteColor);
    LoadColor(KEY_HEADER_COLOR, gSettings.headerColor);
    LoadColor(KEY_CATEGORY_COLOR, gSettings.categoryColor);

    // -------------------------------------------------------------------------
    // Step 5: Load layout preferences
    // -------------------------------------------------------------------------
    int32_t layoutTemp;
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_FONT_SCALE, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.layoutPrefs.fontScale = layoutTemp;
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_LIST_WIDTH, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.layoutPrefs.listWidthPercent = layoutTemp;
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_ICON_SIZE, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.layoutPrefs.iconSizePercent = layoutTemp;
    }

    // Apply loaded layout preferences to the layout system
    Layout::SetCurrentPreferences(gSettings.layoutPrefs);

    // -------------------------------------------------------------------------
    // Step 6: Load next category ID
    // -------------------------------------------------------------------------
    int32_t nextId;
    if (WUPSStorageAPI_GetInt(nullptr, KEY_NEXT_CAT_ID, &nextId) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.nextCategoryId = static_cast<uint16_t>(nextId);
    }

    // -------------------------------------------------------------------------
    // Step 7: Load favorites
    // -------------------------------------------------------------------------
    // Favorites are stored as a binary blob (array of uint64_t)
    int32_t favCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_FAVORITES_COUNT, &favCount);

    if (favCount > 0 && favCount <= MAX_FAVORITES) {
        // Allocate temporary buffer for loading
        uint64_t* favData = new uint64_t[favCount];
        uint32_t maxSize = favCount * sizeof(uint64_t);
        uint32_t readSize = 0;

        if (WUPSStorageAPI_GetBinary(nullptr, KEY_FAVORITES_DATA, favData,
                                      maxSize, &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            gSettings.favorites.clear();
            for (int32_t i = 0; i < favCount; i++) {
                gSettings.favorites.push_back(favData[i]);
            }
        }

        delete[] favData;
    }

    // -------------------------------------------------------------------------
    // Step 8: Load categories
    // -------------------------------------------------------------------------
    int32_t catCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_CATEGORIES_COUNT, &catCount);

    if (catCount > 0 && catCount <= MAX_CATEGORIES) {
        Category* catData = new Category[catCount];
        uint32_t maxSize = catCount * sizeof(Category);
        uint32_t readSize = 0;

        if (WUPSStorageAPI_GetBinary(nullptr, KEY_CATEGORIES_DATA, catData,
                                      maxSize, &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            gSettings.categories.clear();
            for (int32_t i = 0; i < catCount; i++) {
                gSettings.categories.push_back(catData[i]);
            }
        }

        delete[] catData;
    }

    // -------------------------------------------------------------------------
    // Step 9: Load title-category assignments
    // -------------------------------------------------------------------------
    int32_t tcCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_TITLE_CAT_COUNT, &tcCount);

    if (tcCount > 0 && tcCount <= MAX_TITLE_CATEGORIES) {
        TitleCategoryAssignment* tcData = new TitleCategoryAssignment[tcCount];
        uint32_t maxSize = tcCount * sizeof(TitleCategoryAssignment);
        uint32_t readSize = 0;

        if (WUPSStorageAPI_GetBinary(nullptr, KEY_TITLE_CAT_DATA, tcData,
                                      maxSize, &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            gSettings.titleCategories.clear();
            for (int32_t i = 0; i < tcCount; i++) {
                gSettings.titleCategories.push_back(tcData[i]);
            }
        }

        delete[] tcData;
    }

    gSettings.configVersion = version;
}

void Save()
{
    // -------------------------------------------------------------------------
    // Save version
    // -------------------------------------------------------------------------
    WUPSStorageAPI_StoreInt(nullptr, KEY_VERSION, CONFIG_VERSION);

    // -------------------------------------------------------------------------
    // Save integer settings
    // -------------------------------------------------------------------------
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAST_INDEX, gSettings.lastIndex);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAST_CATEGORY, gSettings.lastCategoryIndex);
    WUPSStorageAPI_StoreInt(nullptr, KEY_NEXT_CAT_ID, gSettings.nextCategoryId);

    // -------------------------------------------------------------------------
    // Save boolean settings (stored as int: 0=false, 1=true)
    // -------------------------------------------------------------------------
    WUPSStorageAPI_StoreInt(nullptr, KEY_SHOW_NUMBERS, gSettings.showNumbers ? 1 : 0);
    WUPSStorageAPI_StoreInt(nullptr, KEY_SHOW_FAVORITES, gSettings.showFavorites ? 1 : 0);

    // -------------------------------------------------------------------------
    // Save colors (cast to signed for storage)
    // -------------------------------------------------------------------------
    WUPSStorageAPI_StoreInt(nullptr, KEY_BG_COLOR, static_cast<int32_t>(gSettings.bgColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_TITLE_COLOR, static_cast<int32_t>(gSettings.titleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HIGHLIGHTED, static_cast<int32_t>(gSettings.highlightedTitleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITE_COLOR, static_cast<int32_t>(gSettings.favoriteColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HEADER_COLOR, static_cast<int32_t>(gSettings.headerColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_CATEGORY_COLOR, static_cast<int32_t>(gSettings.categoryColor));

    // -------------------------------------------------------------------------
    // Save layout preferences
    // -------------------------------------------------------------------------
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_FONT_SCALE, gSettings.layoutPrefs.fontScale);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_LIST_WIDTH, gSettings.layoutPrefs.listWidthPercent);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_ICON_SIZE, gSettings.layoutPrefs.iconSizePercent);

    // -------------------------------------------------------------------------
    // Save favorites
    // -------------------------------------------------------------------------
    int32_t favCount = static_cast<int32_t>(gSettings.favorites.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITES_COUNT, favCount);

    if (favCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_FAVORITES_DATA,
                                   gSettings.favorites.data(),
                                   favCount * sizeof(uint64_t));
    }

    // -------------------------------------------------------------------------
    // Save categories
    // -------------------------------------------------------------------------
    int32_t catCount = static_cast<int32_t>(gSettings.categories.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_CATEGORIES_COUNT, catCount);

    if (catCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_CATEGORIES_DATA,
                                   gSettings.categories.data(),
                                   catCount * sizeof(Category));
    }

    // -------------------------------------------------------------------------
    // Save title-category assignments
    // -------------------------------------------------------------------------
    int32_t tcCount = static_cast<int32_t>(gSettings.titleCategories.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_TITLE_CAT_COUNT, tcCount);

    if (tcCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_TITLE_CAT_DATA,
                                   gSettings.titleCategories.data(),
                                   tcCount * sizeof(TitleCategoryAssignment));
    }

    // -------------------------------------------------------------------------
    // Flush to SD card
    // -------------------------------------------------------------------------
    WUPSStorageAPI_SaveStorage(false);
}

PluginSettings& Get()
{
    return gSettings;
}

void ResetToDefaults()
{
    gSettings = PluginSettings();
}

// =============================================================================
// Favorites Implementation
// =============================================================================

bool IsFavorite(uint64_t titleId)
{
    return std::find(gSettings.favorites.begin(),
                     gSettings.favorites.end(),
                     titleId) != gSettings.favorites.end();
}

void ToggleFavorite(uint64_t titleId)
{
    if (IsFavorite(titleId)) {
        RemoveFavorite(titleId);
    } else {
        AddFavorite(titleId);
    }
}

void AddFavorite(uint64_t titleId)
{
    // Check if already favorited
    if (IsFavorite(titleId)) {
        return;
    }

    // Check if we're at the limit
    if (gSettings.favorites.size() >= MAX_FAVORITES) {
        return;
    }

    gSettings.favorites.push_back(titleId);
}

void RemoveFavorite(uint64_t titleId)
{
    auto it = std::find(gSettings.favorites.begin(),
                        gSettings.favorites.end(),
                        titleId);

    if (it != gSettings.favorites.end()) {
        gSettings.favorites.erase(it);
    }
}

// =============================================================================
// Category Implementation
// =============================================================================

uint16_t CreateCategory(const char* name)
{
    // Check if we're at the limit
    if (gSettings.categories.size() >= MAX_CATEGORIES) {
        return 0;
    }

    Category newCat;
    newCat.id = gSettings.nextCategoryId++;

    // Copy name, ensuring null termination
    strncpy(newCat.name, name, MAX_CATEGORY_NAME - 1);
    newCat.name[MAX_CATEGORY_NAME - 1] = '\0';

    gSettings.categories.push_back(newCat);
    return newCat.id;
}

void DeleteCategory(uint16_t categoryId)
{
    // Remove the category
    auto catIt = std::find_if(gSettings.categories.begin(),
                               gSettings.categories.end(),
                               [categoryId](const Category& c) {
                                   return c.id == categoryId;
                               });

    if (catIt != gSettings.categories.end()) {
        gSettings.categories.erase(catIt);
    }

    // Remove all title assignments for this category
    gSettings.titleCategories.erase(
        std::remove_if(gSettings.titleCategories.begin(),
                       gSettings.titleCategories.end(),
                       [categoryId](const TitleCategoryAssignment& tc) {
                           return tc.categoryId == categoryId;
                       }),
        gSettings.titleCategories.end()
    );
}

void RenameCategory(uint16_t categoryId, const char* newName)
{
    for (auto& cat : gSettings.categories) {
        if (cat.id == categoryId) {
            strncpy(cat.name, newName, MAX_CATEGORY_NAME - 1);
            cat.name[MAX_CATEGORY_NAME - 1] = '\0';
            return;
        }
    }
}

const Category* GetCategory(uint16_t categoryId)
{
    for (const auto& cat : gSettings.categories) {
        if (cat.id == categoryId) {
            return &cat;
        }
    }
    return nullptr;
}

int GetCategoryCount()
{
    return static_cast<int>(gSettings.categories.size());
}

bool TitleHasCategory(uint64_t titleId, uint16_t categoryId)
{
    for (const auto& tc : gSettings.titleCategories) {
        if (tc.titleId == titleId && tc.categoryId == categoryId) {
            return true;
        }
    }
    return false;
}

void AssignTitleToCategory(uint64_t titleId, uint16_t categoryId)
{
    // Check if already assigned
    if (TitleHasCategory(titleId, categoryId)) {
        return;
    }

    // Check limit
    if (gSettings.titleCategories.size() >= MAX_TITLE_CATEGORIES) {
        return;
    }

    TitleCategoryAssignment assignment;
    assignment.titleId = titleId;
    assignment.categoryId = categoryId;
    gSettings.titleCategories.push_back(assignment);
}

void RemoveTitleFromCategory(uint64_t titleId, uint16_t categoryId)
{
    gSettings.titleCategories.erase(
        std::remove_if(gSettings.titleCategories.begin(),
                       gSettings.titleCategories.end(),
                       [titleId, categoryId](const TitleCategoryAssignment& tc) {
                           return tc.titleId == titleId && tc.categoryId == categoryId;
                       }),
        gSettings.titleCategories.end()
    );
}

int GetCategoriesForTitle(uint64_t titleId, uint16_t* outIds, int maxIds)
{
    int count = 0;
    for (const auto& tc : gSettings.titleCategories) {
        if (tc.titleId == titleId && count < maxIds) {
            outIds[count++] = tc.categoryId;
        }
    }
    return count;
}

// =============================================================================
// Category Visibility and Ordering Implementation
// =============================================================================

void SetCategoryHidden(uint16_t categoryId, bool hidden)
{
    for (auto& cat : gSettings.categories) {
        if (cat.id == categoryId) {
            cat.hidden = hidden;
            return;
        }
    }
}

bool IsCategoryHidden(uint16_t categoryId)
{
    for (const auto& cat : gSettings.categories) {
        if (cat.id == categoryId) {
            return cat.hidden;
        }
    }
    return false;
}

void MoveCategoryUp(uint16_t categoryId)
{
    auto& cats = gSettings.categories;

    // Find the category
    int idx = -1;
    for (size_t i = 0; i < cats.size(); i++) {
        if (cats[i].id == categoryId) {
            idx = static_cast<int>(i);
            break;
        }
    }

    if (idx <= 0) return;  // Already at top or not found

    // Swap with previous category
    std::swap(cats[idx], cats[idx - 1]);
}

void MoveCategoryDown(uint16_t categoryId)
{
    auto& cats = gSettings.categories;

    // Find the category
    int idx = -1;
    for (size_t i = 0; i < cats.size(); i++) {
        if (cats[i].id == categoryId) {
            idx = static_cast<int>(i);
            break;
        }
    }

    if (idx < 0 || idx >= static_cast<int>(cats.size()) - 1) return;  // At bottom or not found

    // Swap with next category
    std::swap(cats[idx], cats[idx + 1]);
}

int GetSortedCategoryIndices(int* outIndices, int maxCount, bool includeHidden)
{
    int count = 0;
    const auto& cats = gSettings.categories;

    for (size_t i = 0; i < cats.size() && count < maxCount; i++) {
        if (includeHidden || !cats[i].hidden) {
            outIndices[count++] = static_cast<int>(i);
        }
    }

    return count;
}

} // namespace Settings
