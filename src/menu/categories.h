/**
 * Category Filter System
 *
 * This module manages the category bar and title filtering in the menu.
 * It provides the filtered list of titles based on the currently selected category.
 *
 * BUILT-IN CATEGORIES:
 * --------------------
 * These categories are always present and cannot be deleted:
 *
 * - "All" (index 0): Shows all titles, no filtering
 * - "Favorites" (index 1): Shows only favorited titles
 *
 * USER CATEGORIES:
 * ----------------
 * Additional categories can be created by the user (stored via Settings).
 * These appear after the built-in categories in the category bar.
 *
 * USAGE:
 * ------
 *   // Initialize when menu opens
 *   Categories::Init();
 *
 *   // Navigate categories
 *   Categories::NextCategory();
 *   Categories::PreviousCategory();
 *
 *   // Get filtered titles
 *   int count = Categories::GetFilteredCount();
 *   for (int i = 0; i < count; i++) {
 *       const TitleInfo* title = Categories::GetFilteredTitle(i);
 *       // ... display title
 *   }
 *
 *   // After changing favorites or assignments, refresh the filter
 *   Categories::RefreshFilter();
 */

#pragma once

#include "../titles/titles.h"

namespace Categories {

// =============================================================================
// Special Category Indices
// =============================================================================

// Built-in category indices (these are always present)
constexpr int CATEGORY_ALL = 0;
constexpr int CATEGORY_FAVORITES = 1;
constexpr int CATEGORY_SYSTEM = 2;      // System apps (title ID 0x00050010...)
constexpr int FIRST_USER_CATEGORY = 3;  // User categories start here

// =============================================================================
// Initialization
// =============================================================================

/**
 * Initialize the category system.
 *
 * This sets up the filtered title list based on the current category.
 * Call this when the menu opens, after Titles::Load() has been called.
 */
void Init();

// =============================================================================
// Category Navigation
// =============================================================================

/**
 * Get the total number of categories (built-in + user-defined).
 *
 * @return Number of categories
 */
int GetTotalCategoryCount();

/**
 * Get the number of visible categories (excludes hidden user categories).
 *
 * @return Number of visible categories
 */
int GetVisibleCategoryCount();

/**
 * Check if a category is visible (not hidden).
 *
 * @param index Category index
 * @return true if visible, false if hidden
 */
bool IsCategoryVisible(int index);

/**
 * Get the index of the currently selected category.
 *
 * @return Current category index (0 = All, 1 = Favorites, 2+ = user)
 */
int GetCurrentCategoryIndex();

/**
 * Get the display name of a category.
 *
 * @param index Category index
 * @return Display name, or nullptr if index is invalid
 */
const char* GetCategoryName(int index);

/**
 * Select the next category (cycle right).
 *
 * Wraps around from last to first.
 * Automatically refreshes the filtered title list.
 */
void NextCategory();

/**
 * Select the previous category (cycle left).
 *
 * Wraps around from first to last.
 * Automatically refreshes the filtered title list.
 */
void PreviousCategory();

/**
 * Select a specific category by index.
 *
 * @param index Category index to select
 * @return true if the category was selected, false if index invalid
 */
bool SelectCategory(int index);

// =============================================================================
// Filtered Title Access
// =============================================================================

/**
 * Get the number of titles matching the current category filter.
 *
 * @return Number of filtered titles
 */
int GetFilteredCount();

/**
 * Get a filtered title by index.
 *
 * @param index Index in the filtered list (0 to GetFilteredCount()-1)
 * @return Pointer to TitleInfo, or nullptr if index is out of range
 */
const Titles::TitleInfo* GetFilteredTitle(int index);

/**
 * Refresh the filtered title list.
 *
 * Call this after modifying favorites or category assignments
 * while staying in the same category. The filter will be reapplied.
 */
void RefreshFilter();

} // namespace Categories
