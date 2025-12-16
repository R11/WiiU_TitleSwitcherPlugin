/**
 * Categories Stub for Desktop Preview
 *
 * Provides mock category filtering for testing the menu rendering.
 */

#pragma once

#include "titles_stub.h"
#include "settings_stub.h"
#include <vector>
#include <cstring>

namespace Categories {

constexpr int CATEGORY_ALL = 0;
constexpr int CATEGORY_FAVORITES = 1;
constexpr int CATEGORY_SYSTEM = 2;
constexpr int FIRST_USER_CATEGORY = 3;

// Internal state
inline int sCurrentCategory = 0;
inline std::vector<int> sFilteredIndices;

inline void RefreshFilter() {
    sFilteredIndices.clear();

    int titleCount = Titles::GetCount();

    for (int i = 0; i < titleCount; i++) {
        const Titles::TitleInfo* title = Titles::GetTitle(i);
        if (!title) continue;

        bool include = false;

        if (sCurrentCategory == CATEGORY_ALL) {
            // All titles
            include = true;
        } else if (sCurrentCategory == CATEGORY_FAVORITES) {
            // Only favorites
            include = Settings::IsFavorite(title->titleId);
        } else if (sCurrentCategory == CATEGORY_SYSTEM) {
            // System apps (title ID starts with 0x00050010)
            include = ((title->titleId >> 32) == 0x00050010);
        } else {
            // User category
            int userCatIndex = sCurrentCategory - FIRST_USER_CATEGORY;
            if (userCatIndex >= 0 && userCatIndex < Settings::GetCategoryCount()) {
                uint16_t catId = Settings::Get().categories[userCatIndex].id;
                include = Settings::TitleHasCategory(title->titleId, catId);
            }
        }

        if (include) {
            sFilteredIndices.push_back(i);
        }
    }
}

inline void Init() {
    sCurrentCategory = 0;
    RefreshFilter();
}

inline int GetTotalCategoryCount() {
    // Built-in (All, Favorites, System) + user categories
    return FIRST_USER_CATEGORY + Settings::GetCategoryCount();
}

inline int GetVisibleCategoryCount() {
    int count = FIRST_USER_CATEGORY;  // Built-in are always visible
    for (const auto& cat : Settings::Get().categories) {
        if (!cat.hidden) count++;
    }
    return count;
}

inline bool IsCategoryVisible(int index) {
    if (index < FIRST_USER_CATEGORY) return true;  // Built-in always visible

    int userIndex = index - FIRST_USER_CATEGORY;
    if (userIndex >= 0 && userIndex < Settings::GetCategoryCount()) {
        return !Settings::Get().categories[userIndex].hidden;
    }
    return false;
}

inline int GetCurrentCategoryIndex() {
    return sCurrentCategory;
}

inline const char* GetCategoryName(int index) {
    switch (index) {
        case CATEGORY_ALL: return "All";
        case CATEGORY_FAVORITES: return "Favorites";
        case CATEGORY_SYSTEM: return "System";
        default: {
            int userIndex = index - FIRST_USER_CATEGORY;
            if (userIndex >= 0 && userIndex < Settings::GetCategoryCount()) {
                return Settings::Get().categories[userIndex].name;
            }
            return nullptr;
        }
    }
}

inline void NextCategory() {
    int total = GetTotalCategoryCount();
    do {
        sCurrentCategory = (sCurrentCategory + 1) % total;
    } while (!IsCategoryVisible(sCurrentCategory) && sCurrentCategory != 0);
    RefreshFilter();
}

inline void PreviousCategory() {
    int total = GetTotalCategoryCount();
    do {
        sCurrentCategory = (sCurrentCategory - 1 + total) % total;
    } while (!IsCategoryVisible(sCurrentCategory) && sCurrentCategory != 0);
    RefreshFilter();
}

inline bool SelectCategory(int index) {
    if (index < 0 || index >= GetTotalCategoryCount()) return false;
    sCurrentCategory = index;
    RefreshFilter();
    return true;
}

inline int GetFilteredCount() {
    return sFilteredIndices.size();
}

inline const Titles::TitleInfo* GetFilteredTitle(int index) {
    if (index < 0 || index >= (int)sFilteredIndices.size()) return nullptr;
    return Titles::GetTitle(sFilteredIndices[index]);
}

} // namespace Categories
