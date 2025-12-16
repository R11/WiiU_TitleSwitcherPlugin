/**
 * Settings Stub for Desktop Preview
 *
 * Provides mock settings data for testing the menu rendering.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Settings {

constexpr int32_t CONFIG_VERSION = 2;
constexpr int MAX_FAVORITES = 64;
constexpr int MAX_CATEGORIES = 16;
constexpr int MAX_CATEGORY_NAME = 32;
constexpr int MAX_TITLE_CATEGORIES = 512;

// Default colors (Catppuccin Mocha)
constexpr uint32_t DEFAULT_BG_COLOR          = 0x1E1E2EFF;
constexpr uint32_t DEFAULT_TITLE_COLOR       = 0xCDD6F4FF;
constexpr uint32_t DEFAULT_HIGHLIGHTED_COLOR = 0x89B4FAFF;
constexpr uint32_t DEFAULT_FAVORITE_COLOR    = 0xF9E2AFFF;
constexpr uint32_t DEFAULT_HEADER_COLOR      = 0xA6E3A1FF;
constexpr uint32_t DEFAULT_CATEGORY_COLOR    = 0xF5C2E7FF;

struct Category {
    uint16_t id;
    uint16_t order;
    char name[MAX_CATEGORY_NAME];
    bool hidden;
    uint8_t _padding[3];

    Category() : id(0), order(0), hidden(false) {
        name[0] = '\0';
        _padding[0] = _padding[1] = _padding[2] = 0;
    }
};

struct TitleCategoryAssignment {
    uint64_t titleId;
    uint16_t categoryId;
    uint16_t _padding;

    TitleCategoryAssignment() : titleId(0), categoryId(0), _padding(0) {}
};

struct PluginSettings {
    int32_t configVersion;
    int32_t lastIndex;
    int32_t lastCategoryIndex;
    bool showNumbers;
    bool showFavorites;
    uint32_t bgColor;
    uint32_t titleColor;
    uint32_t highlightedTitleColor;
    uint32_t favoriteColor;
    uint32_t headerColor;
    uint32_t categoryColor;
    std::vector<uint64_t> favorites;
    std::vector<Category> categories;
    std::vector<TitleCategoryAssignment> titleCategories;
    uint16_t nextCategoryId;

    PluginSettings() :
        configVersion(CONFIG_VERSION),
        lastIndex(0),
        lastCategoryIndex(0),
        showNumbers(false),
        showFavorites(true),
        bgColor(DEFAULT_BG_COLOR),
        titleColor(DEFAULT_TITLE_COLOR),
        highlightedTitleColor(DEFAULT_HIGHLIGHTED_COLOR),
        favoriteColor(DEFAULT_FAVORITE_COLOR),
        headerColor(DEFAULT_HEADER_COLOR),
        categoryColor(DEFAULT_CATEGORY_COLOR),
        nextCategoryId(1)
    {}
};

// Global settings instance
inline PluginSettings sSettings;
inline bool sInitialized = false;

inline void Init() {
    sSettings = PluginSettings();

    // Add some mock categories
    Category games;
    games.id = 1;
    games.order = 0;
    strncpy(games.name, "Games", MAX_CATEGORY_NAME);
    sSettings.categories.push_back(games);

    Category apps;
    apps.id = 2;
    apps.order = 1;
    strncpy(apps.name, "Apps", MAX_CATEGORY_NAME);
    sSettings.categories.push_back(apps);

    sSettings.nextCategoryId = 3;

    // Add some mock favorites
    sSettings.favorites.push_back(0x0005000010145D00); // Super Mario 3D World
    sSettings.favorites.push_back(0x000500001010EC00); // Mario Kart 8
    sSettings.favorites.push_back(0x0005000010145000); // Smash Bros

    // Add some mock category assignments
    TitleCategoryAssignment a1;
    a1.titleId = 0x0005000010145D00;
    a1.categoryId = 1;
    sSettings.titleCategories.push_back(a1);

    TitleCategoryAssignment a2;
    a2.titleId = 0x000500001010EC00;
    a2.categoryId = 1;
    sSettings.titleCategories.push_back(a2);

    sInitialized = true;
}

inline void Load() {}
inline void Save() {}

inline PluginSettings& Get() {
    if (!sInitialized) Init();
    return sSettings;
}

inline void ResetToDefaults() {
    sSettings = PluginSettings();
}

inline bool IsFavorite(uint64_t titleId) {
    auto& favs = Get().favorites;
    return std::find(favs.begin(), favs.end(), titleId) != favs.end();
}

inline void ToggleFavorite(uint64_t titleId) {
    auto& favs = Get().favorites;
    auto it = std::find(favs.begin(), favs.end(), titleId);
    if (it != favs.end()) {
        favs.erase(it);
    } else {
        favs.push_back(titleId);
    }
}

inline void AddFavorite(uint64_t titleId) {
    if (!IsFavorite(titleId)) {
        Get().favorites.push_back(titleId);
    }
}

inline void RemoveFavorite(uint64_t titleId) {
    auto& favs = Get().favorites;
    favs.erase(std::remove(favs.begin(), favs.end(), titleId), favs.end());
}

inline uint16_t CreateCategory(const char* name) {
    auto& cats = Get().categories;
    if (cats.size() >= MAX_CATEGORIES) return 0;

    Category cat;
    cat.id = Get().nextCategoryId++;
    cat.order = cats.size();
    strncpy(cat.name, name, MAX_CATEGORY_NAME - 1);
    cat.name[MAX_CATEGORY_NAME - 1] = '\0';
    cats.push_back(cat);
    return cat.id;
}

inline void DeleteCategory(uint16_t categoryId) {
    auto& cats = Get().categories;
    cats.erase(std::remove_if(cats.begin(), cats.end(),
        [categoryId](const Category& c) { return c.id == categoryId; }), cats.end());

    auto& assigns = Get().titleCategories;
    assigns.erase(std::remove_if(assigns.begin(), assigns.end(),
        [categoryId](const TitleCategoryAssignment& a) { return a.categoryId == categoryId; }), assigns.end());
}

inline void RenameCategory(uint16_t categoryId, const char* newName) {
    for (auto& cat : Get().categories) {
        if (cat.id == categoryId) {
            strncpy(cat.name, newName, MAX_CATEGORY_NAME - 1);
            cat.name[MAX_CATEGORY_NAME - 1] = '\0';
            break;
        }
    }
}

inline const Category* GetCategory(uint16_t categoryId) {
    for (const auto& cat : Get().categories) {
        if (cat.id == categoryId) return &cat;
    }
    return nullptr;
}

inline int GetCategoryCount() {
    return Get().categories.size();
}

inline bool TitleHasCategory(uint64_t titleId, uint16_t categoryId) {
    for (const auto& a : Get().titleCategories) {
        if (a.titleId == titleId && a.categoryId == categoryId) return true;
    }
    return false;
}

inline void AssignTitleToCategory(uint64_t titleId, uint16_t categoryId) {
    if (TitleHasCategory(titleId, categoryId)) return;
    TitleCategoryAssignment a;
    a.titleId = titleId;
    a.categoryId = categoryId;
    Get().titleCategories.push_back(a);
}

inline void RemoveTitleFromCategory(uint64_t titleId, uint16_t categoryId) {
    auto& assigns = Get().titleCategories;
    assigns.erase(std::remove_if(assigns.begin(), assigns.end(),
        [titleId, categoryId](const TitleCategoryAssignment& a) {
            return a.titleId == titleId && a.categoryId == categoryId;
        }), assigns.end());
}

inline int GetCategoriesForTitle(uint64_t titleId, uint16_t* outIds, int maxIds) {
    int count = 0;
    for (const auto& a : Get().titleCategories) {
        if (a.titleId == titleId && count < maxIds) {
            outIds[count++] = a.categoryId;
        }
    }
    return count;
}

inline void SetCategoryHidden(uint16_t categoryId, bool hidden) {
    for (auto& cat : Get().categories) {
        if (cat.id == categoryId) {
            cat.hidden = hidden;
            break;
        }
    }
}

inline bool IsCategoryHidden(uint16_t categoryId) {
    for (const auto& cat : Get().categories) {
        if (cat.id == categoryId) return cat.hidden;
    }
    return false;
}

inline void MoveCategoryUp(uint16_t categoryId) { (void)categoryId; }
inline void MoveCategoryDown(uint16_t categoryId) { (void)categoryId; }

inline int GetSortedCategoryIndices(int* outIndices, int maxCount, bool includeHidden = true) {
    int count = 0;
    for (size_t i = 0; i < Get().categories.size() && count < maxCount; i++) {
        if (includeHidden || !Get().categories[i].hidden) {
            outIndices[count++] = i;
        }
    }
    return count;
}

} // namespace Settings
