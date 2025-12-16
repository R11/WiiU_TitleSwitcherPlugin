/**
 * Category Filter Implementation
 *
 * See categories.h for usage documentation.
 */

#include "categories.h"
#include "../titles/titles.h"
#include "../storage/settings.h"

namespace Categories {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Current category selection
int sCurrentCategory = CATEGORY_ALL;

// Filtered title indices
// These are indices into the main Titles list, not the filtered list
// This allows us to maintain the connection to the original title data
int sFilteredIndices[Titles::MAX_TITLES];
int sFilteredCount = 0;

// Built-in category names
constexpr const char* NAME_ALL = "All";
constexpr const char* NAME_FAVORITES = "Favorites";
constexpr const char* NAME_SYSTEM = "System";

// Title ID prefix for system apps (0x00050010XXXXXXXX)
constexpr uint64_t SYSTEM_APP_PREFIX = 0x0005001000000000ULL;
constexpr uint64_t SYSTEM_APP_MASK   = 0xFFFFFF0000000000ULL;

/**
 * Check if a title ID is a system app.
 */
bool isSystemApp(uint64_t titleId)
{
    return (titleId & SYSTEM_APP_MASK) == SYSTEM_APP_PREFIX;
}

/**
 * Apply the current category filter to rebuild the filtered list.
 */
void applyFilter()
{
    sFilteredCount = 0;
    int totalTitles = Titles::GetCount();

    for (int i = 0; i < totalTitles; i++) {
        const Titles::TitleInfo* title = Titles::GetTitle(i);
        if (!title) continue;

        bool include = false;

        switch (sCurrentCategory) {
            case CATEGORY_ALL:
                // All category includes everything
                include = true;
                break;

            case CATEGORY_FAVORITES:
                // Favorites category: only include favorited titles
                include = Settings::IsFavorite(title->titleId);
                break;

            case CATEGORY_SYSTEM:
                // System category: only include system apps
                include = isSystemApp(title->titleId);
                break;

            default:
                // User-defined category: check if title is assigned
                {
                    // Get the category ID for this index
                    int userCatIndex = sCurrentCategory - FIRST_USER_CATEGORY;
                    if (userCatIndex >= 0 && userCatIndex < Settings::GetCategoryCount()) {
                        const auto& categories = Settings::Get().categories;
                        if (userCatIndex < static_cast<int>(categories.size())) {
                            uint16_t catId = categories[userCatIndex].id;
                            include = Settings::TitleHasCategory(title->titleId, catId);
                        }
                    }
                }
                break;
        }

        if (include) {
            sFilteredIndices[sFilteredCount++] = i;
        }
    }
}

} // anonymous namespace

// =============================================================================
// Public Implementation
// =============================================================================

void Init()
{
    // Restore last category from settings
    sCurrentCategory = Settings::Get().lastCategoryIndex;

    // Ensure it's valid
    int maxCategory = GetTotalCategoryCount() - 1;
    if (sCurrentCategory < 0) {
        sCurrentCategory = 0;
    }
    if (sCurrentCategory > maxCategory) {
        sCurrentCategory = 0;
    }

    // Apply the filter
    applyFilter();
}

int GetTotalCategoryCount()
{
    // Built-in (All, Favorites, System) + user-defined
    return FIRST_USER_CATEGORY + Settings::GetCategoryCount();
}

int GetVisibleCategoryCount()
{
    int count = FIRST_USER_CATEGORY;  // Built-in categories are always visible
    const auto& categories = Settings::Get().categories;
    for (const auto& cat : categories) {
        if (!cat.hidden) {
            count++;
        }
    }
    return count;
}

bool IsCategoryVisible(int index)
{
    // Built-in categories are always visible
    if (index < FIRST_USER_CATEGORY) {
        return true;
    }

    // Check user-defined category
    int userIndex = index - FIRST_USER_CATEGORY;
    const auto& categories = Settings::Get().categories;
    if (userIndex >= 0 && userIndex < static_cast<int>(categories.size())) {
        return !categories[userIndex].hidden;
    }
    return false;
}

int GetCurrentCategoryIndex()
{
    return sCurrentCategory;
}

const char* GetCategoryName(int index)
{
    if (index < 0) {
        return nullptr;
    }

    // Built-in categories
    if (index == CATEGORY_ALL) {
        return NAME_ALL;
    }
    if (index == CATEGORY_FAVORITES) {
        return NAME_FAVORITES;
    }
    if (index == CATEGORY_SYSTEM) {
        return NAME_SYSTEM;
    }

    // User-defined categories
    int userIndex = index - FIRST_USER_CATEGORY;
    const auto& categories = Settings::Get().categories;

    if (userIndex >= 0 && userIndex < static_cast<int>(categories.size())) {
        return categories[userIndex].name;
    }

    return nullptr;
}

void NextCategory()
{
    int total = GetTotalCategoryCount();
    sCurrentCategory++;
    if (sCurrentCategory >= total) {
        sCurrentCategory = 0;  // Wrap around
    }

    // Save selection
    Settings::Get().lastCategoryIndex = sCurrentCategory;

    // Refresh filter
    applyFilter();
}

void PreviousCategory()
{
    int total = GetTotalCategoryCount();
    sCurrentCategory--;
    if (sCurrentCategory < 0) {
        sCurrentCategory = total - 1;  // Wrap around
    }

    // Save selection
    Settings::Get().lastCategoryIndex = sCurrentCategory;

    // Refresh filter
    applyFilter();
}

bool SelectCategory(int index)
{
    if (index < 0 || index >= GetTotalCategoryCount()) {
        return false;
    }

    sCurrentCategory = index;
    Settings::Get().lastCategoryIndex = sCurrentCategory;
    applyFilter();
    return true;
}

int GetFilteredCount()
{
    return sFilteredCount;
}

const Titles::TitleInfo* GetFilteredTitle(int index)
{
    if (index < 0 || index >= sFilteredCount) {
        return nullptr;
    }

    // Get the original index and return that title
    int originalIndex = sFilteredIndices[index];
    return Titles::GetTitle(originalIndex);
}

void RefreshFilter()
{
    applyFilter();
}

} // namespace Categories
