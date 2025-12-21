/**
 * Mock Settings for Web Preview
 *
 * Provides a minimal Settings implementation for preview purposes.
 */

#include "storage/settings.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

namespace Settings {

// Static settings instance
static PluginSettings sSettings;
static bool sInitialized = false;

/**
 * Initialize settings with defaults
 */
void initDefaults() {
    if (sInitialized) return;

    // Set default colors
    sSettings.bgColor = DEFAULT_BG_COLOR;
    sSettings.titleColor = DEFAULT_TITLE_COLOR;
    sSettings.highlightedTitleColor = DEFAULT_HIGHLIGHTED_COLOR;
    sSettings.favoriteColor = DEFAULT_FAVORITE_COLOR;
    sSettings.headerColor = DEFAULT_HEADER_COLOR;
    sSettings.categoryColor = DEFAULT_CATEGORY_COLOR;

    // Default options
    sSettings.showFavorites = true;
    sSettings.showNumbers = false;
    sSettings.lastIndex = 0;
    sSettings.lastCategoryIndex = 0;
    sSettings.nextCategoryId = 1;

    // Sample favorites
    sSettings.favorites.clear();
    sSettings.favorites.push_back(0x0005000010101D00);  // Super Mario 3D World
    sSettings.favorites.push_back(0x000500001010EC00);  // Mario Kart 8
    sSettings.favorites.push_back(0x0005000010145000);  // Super Smash Bros

    // Sample categories
    sSettings.categories.clear();

    Category games;
    games.id = sSettings.nextCategoryId++;
    games.order = 0;
    games.hidden = false;
    strncpy(games.name, "Games", MAX_CATEGORY_NAME - 1);
    games.name[MAX_CATEGORY_NAME - 1] = '\0';
    sSettings.categories.push_back(games);

    Category apps;
    apps.id = sSettings.nextCategoryId++;
    apps.order = 1;
    apps.hidden = false;
    strncpy(apps.name, "Apps", MAX_CATEGORY_NAME - 1);
    apps.name[MAX_CATEGORY_NAME - 1] = '\0';
    sSettings.categories.push_back(apps);

    sSettings.titleCategories.clear();

    sInitialized = true;
}

/**
 * Initialize the settings system
 */
void Init() {
    initDefaults();
}

/**
 * Get settings reference
 */
PluginSettings& Get() {
    initDefaults();
    return sSettings;
}

/**
 * Load/Save stubs
 */
void Load() { initDefaults(); }
void Save() { /* No-op for web preview */ }

/**
 * Favorites management
 */
bool IsFavorite(uint64_t titleId) {
    for (const auto& fav : sSettings.favorites) {
        if (fav == titleId) return true;
    }
    return false;
}

void AddFavorite(uint64_t titleId) {
    if ((int)sSettings.favorites.size() < MAX_FAVORITES && !IsFavorite(titleId)) {
        sSettings.favorites.push_back(titleId);
    }
}

void RemoveFavorite(uint64_t titleId) {
    auto it = std::find(sSettings.favorites.begin(), sSettings.favorites.end(), titleId);
    if (it != sSettings.favorites.end()) {
        sSettings.favorites.erase(it);
    }
}

void ToggleFavorite(uint64_t titleId) {
    if (IsFavorite(titleId)) {
        RemoveFavorite(titleId);
    } else {
        AddFavorite(titleId);
    }
}

/**
 * Category management
 */
uint16_t CreateCategory(const char* name) {
    if ((int)sSettings.categories.size() >= MAX_CATEGORIES) return 0;

    Category cat;
    cat.id = sSettings.nextCategoryId++;
    cat.order = (uint16_t)sSettings.categories.size();
    cat.hidden = false;
    strncpy(cat.name, name, MAX_CATEGORY_NAME - 1);
    cat.name[MAX_CATEGORY_NAME - 1] = '\0';

    sSettings.categories.push_back(cat);
    return cat.id;
}

void DeleteCategory(uint16_t categoryId) {
    // Remove all title assignments for this category
    auto it = sSettings.titleCategories.begin();
    while (it != sSettings.titleCategories.end()) {
        if (it->categoryId == categoryId) {
            it = sSettings.titleCategories.erase(it);
        } else {
            ++it;
        }
    }

    // Remove the category itself
    for (auto catIt = sSettings.categories.begin(); catIt != sSettings.categories.end(); ++catIt) {
        if (catIt->id == categoryId) {
            sSettings.categories.erase(catIt);
            return;
        }
    }
}

void RenameCategory(uint16_t categoryId, const char* newName) {
    for (auto& cat : sSettings.categories) {
        if (cat.id == categoryId) {
            strncpy(cat.name, newName, MAX_CATEGORY_NAME - 1);
            cat.name[MAX_CATEGORY_NAME - 1] = '\0';
            return;
        }
    }
}

const Category* GetCategory(uint16_t categoryId) {
    for (const auto& cat : sSettings.categories) {
        if (cat.id == categoryId) {
            return &cat;
        }
    }
    return nullptr;
}

int GetCategoryCount() {
    return (int)sSettings.categories.size();
}

int GetSortedCategoryIndices(int* outIndices, int maxCount, bool includeHidden) {
    int count = 0;
    for (size_t i = 0; i < sSettings.categories.size() && count < maxCount; i++) {
        if (includeHidden || !sSettings.categories[i].hidden) {
            outIndices[count++] = (int)i;
        }
    }
    return count;
}

void MoveCategoryUp(uint16_t categoryId) {
    for (size_t i = 1; i < sSettings.categories.size(); i++) {
        if (sSettings.categories[i].id == categoryId) {
            std::swap(sSettings.categories[i], sSettings.categories[i - 1]);
            return;
        }
    }
}

void MoveCategoryDown(uint16_t categoryId) {
    for (size_t i = 0; i + 1 < sSettings.categories.size(); i++) {
        if (sSettings.categories[i].id == categoryId) {
            std::swap(sSettings.categories[i], sSettings.categories[i + 1]);
            return;
        }
    }
}

void SetCategoryHidden(uint16_t categoryId, bool hidden) {
    for (auto& cat : sSettings.categories) {
        if (cat.id == categoryId) {
            cat.hidden = hidden;
            return;
        }
    }
}

bool IsCategoryHidden(uint16_t categoryId) {
    for (const auto& cat : sSettings.categories) {
        if (cat.id == categoryId) {
            return cat.hidden;
        }
    }
    return false;
}

/**
 * Title-category assignments
 */
bool TitleHasCategory(uint64_t titleId, uint16_t categoryId) {
    for (const auto& assign : sSettings.titleCategories) {
        if (assign.titleId == titleId && assign.categoryId == categoryId) {
            return true;
        }
    }
    return false;
}

void AssignTitleToCategory(uint64_t titleId, uint16_t categoryId) {
    if (!TitleHasCategory(titleId, categoryId) &&
        (int)sSettings.titleCategories.size() < MAX_TITLE_CATEGORIES) {
        TitleCategoryAssignment assign;
        assign.titleId = titleId;
        assign.categoryId = categoryId;
        sSettings.titleCategories.push_back(assign);
    }
}

void RemoveTitleFromCategory(uint64_t titleId, uint16_t categoryId) {
    for (auto it = sSettings.titleCategories.begin(); it != sSettings.titleCategories.end(); ++it) {
        if (it->titleId == titleId && it->categoryId == categoryId) {
            sSettings.titleCategories.erase(it);
            return;
        }
    }
}

int GetCategoriesForTitle(uint64_t titleId, uint16_t* outIds, int maxIds) {
    int count = 0;
    for (const auto& assign : sSettings.titleCategories) {
        if (assign.titleId == titleId && count < maxIds) {
            outIds[count++] = assign.categoryId;
        }
    }
    return count;
}

void ResetToDefaults() {
    sInitialized = false;
    sSettings = PluginSettings();
    initDefaults();
}

} // namespace Settings
