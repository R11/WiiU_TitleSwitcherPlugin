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

#include <cstring>
#include <algorithm>

namespace Settings {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// The global settings instance
PluginSettings gSettings;

} // anonymous namespace

// =============================================================================
// Core Functions Implementation
// =============================================================================

void Init()
{
    // Initialize with default values
    gSettings = PluginSettings();
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
