/**
 * Unit tests for src/storage/settings.cpp
 *
 * Tests favorites, categories, and settings management.
 */

#include <gtest/gtest.h>
#include <cstring>
#include "storage/settings.h"

class SettingsTest : public ::testing::Test {
protected:
    void SetUp() override {
        Settings::Init();
    }
};

// =============================================================================
// Favorites Tests
// =============================================================================

TEST_F(SettingsTest, IsFavorite_NotFavorited_ReturnsFalse) {
    EXPECT_FALSE(Settings::IsFavorite(0x0005000010145D00));
}

TEST_F(SettingsTest, AddFavorite_AddsTitleToFavorites) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::AddFavorite(titleId);
    EXPECT_TRUE(Settings::IsFavorite(titleId));
}

TEST_F(SettingsTest, AddFavorite_DuplicateIgnored) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::AddFavorite(titleId);
    Settings::AddFavorite(titleId);
    EXPECT_EQ(Settings::Get().favorites.size(), 1u);
}

TEST_F(SettingsTest, AddFavorite_MultipleTitles) {
    Settings::AddFavorite(0x0005000010145D00);
    Settings::AddFavorite(0x000500001010EC00);
    Settings::AddFavorite(0x0005000010176900);

    EXPECT_TRUE(Settings::IsFavorite(0x0005000010145D00));
    EXPECT_TRUE(Settings::IsFavorite(0x000500001010EC00));
    EXPECT_TRUE(Settings::IsFavorite(0x0005000010176900));
    EXPECT_EQ(Settings::Get().favorites.size(), 3u);
}

TEST_F(SettingsTest, RemoveFavorite_RemovesTitleFromFavorites) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::AddFavorite(titleId);
    Settings::RemoveFavorite(titleId);
    EXPECT_FALSE(Settings::IsFavorite(titleId));
}

TEST_F(SettingsTest, RemoveFavorite_NonexistentIsNoOp) {
    Settings::RemoveFavorite(0x0005000010145D00);
    EXPECT_FALSE(Settings::IsFavorite(0x0005000010145D00));
}

TEST_F(SettingsTest, RemoveFavorite_LeavesOtherFavorites) {
    Settings::AddFavorite(0x0005000010145D00);
    Settings::AddFavorite(0x000500001010EC00);
    Settings::RemoveFavorite(0x0005000010145D00);

    EXPECT_FALSE(Settings::IsFavorite(0x0005000010145D00));
    EXPECT_TRUE(Settings::IsFavorite(0x000500001010EC00));
}

TEST_F(SettingsTest, ToggleFavorite_AddsWhenNotFavorited) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::ToggleFavorite(titleId);
    EXPECT_TRUE(Settings::IsFavorite(titleId));
}

TEST_F(SettingsTest, ToggleFavorite_RemovesWhenFavorited) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::AddFavorite(titleId);
    Settings::ToggleFavorite(titleId);
    EXPECT_FALSE(Settings::IsFavorite(titleId));
}

TEST_F(SettingsTest, ToggleFavorite_DoubleToggleRestoresState) {
    uint64_t titleId = 0x0005000010145D00;
    Settings::ToggleFavorite(titleId);
    Settings::ToggleFavorite(titleId);
    EXPECT_FALSE(Settings::IsFavorite(titleId));
}

TEST_F(SettingsTest, AddFavorite_RespectsMaxLimit) {
    for (int i = 0; i < Settings::MAX_FAVORITES + 10; i++) {
        Settings::AddFavorite(0x0005000010000000 + i);
    }
    EXPECT_LE(Settings::Get().favorites.size(), static_cast<size_t>(Settings::MAX_FAVORITES));
}

// =============================================================================
// Category Creation Tests
// =============================================================================

TEST_F(SettingsTest, CreateCategory_ReturnsValidId) {
    uint16_t id = Settings::CreateCategory("Action");
    EXPECT_GT(id, 0u);
}

TEST_F(SettingsTest, CreateCategory_IncrementingIds) {
    uint16_t id1 = Settings::CreateCategory("Action");
    uint16_t id2 = Settings::CreateCategory("RPG");
    uint16_t id3 = Settings::CreateCategory("Puzzle");

    EXPECT_LT(id1, id2);
    EXPECT_LT(id2, id3);
}

TEST_F(SettingsTest, CreateCategory_CategoryExists) {
    uint16_t id = Settings::CreateCategory("RPG");
    const Settings::Category* cat = Settings::GetCategory(id);

    ASSERT_NE(cat, nullptr);
    EXPECT_STREQ(cat->name, "RPG");
    EXPECT_EQ(cat->id, id);
}

TEST_F(SettingsTest, CreateCategory_CountIncreases) {
    EXPECT_EQ(Settings::GetCategoryCount(), 0);
    Settings::CreateCategory("Cat1");
    EXPECT_EQ(Settings::GetCategoryCount(), 1);
    Settings::CreateCategory("Cat2");
    EXPECT_EQ(Settings::GetCategoryCount(), 2);
}

TEST_F(SettingsTest, CreateCategory_TruncatesLongName) {
    std::string longName(Settings::MAX_CATEGORY_NAME + 20, 'X');
    uint16_t id = Settings::CreateCategory(longName.c_str());

    const Settings::Category* cat = Settings::GetCategory(id);
    ASSERT_NE(cat, nullptr);
    EXPECT_LT(strlen(cat->name), static_cast<size_t>(Settings::MAX_CATEGORY_NAME));
}

TEST_F(SettingsTest, CreateCategory_RespectsMaxLimit) {
    for (int i = 0; i < Settings::MAX_CATEGORIES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Category%d", i);
        EXPECT_GT(Settings::CreateCategory(name), 0u);
    }
    EXPECT_EQ(Settings::CreateCategory("TooMany"), 0u);
    EXPECT_EQ(Settings::GetCategoryCount(), Settings::MAX_CATEGORIES);
}

// =============================================================================
// Category Deletion Tests
// =============================================================================

TEST_F(SettingsTest, DeleteCategory_RemovesCategory) {
    uint16_t id = Settings::CreateCategory("ToDelete");
    EXPECT_NE(Settings::GetCategory(id), nullptr);
    Settings::DeleteCategory(id);
    EXPECT_EQ(Settings::GetCategory(id), nullptr);
}

TEST_F(SettingsTest, DeleteCategory_CountDecreases) {
    uint16_t id1 = Settings::CreateCategory("Cat1");
    Settings::CreateCategory("Cat2");
    EXPECT_EQ(Settings::GetCategoryCount(), 2);
    Settings::DeleteCategory(id1);
    EXPECT_EQ(Settings::GetCategoryCount(), 1);
}

TEST_F(SettingsTest, DeleteCategory_RemovesTitleAssignments) {
    uint16_t catId = Settings::CreateCategory("GameCat");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, catId);
    EXPECT_TRUE(Settings::TitleHasCategory(titleId, catId));

    Settings::DeleteCategory(catId);
    EXPECT_FALSE(Settings::TitleHasCategory(titleId, catId));
}

TEST_F(SettingsTest, DeleteCategory_NonexistentIsNoOp) {
    Settings::DeleteCategory(9999);
    EXPECT_EQ(Settings::GetCategoryCount(), 0);
}

// =============================================================================
// Category Rename Tests
// =============================================================================

TEST_F(SettingsTest, RenameCategory_ChangesName) {
    uint16_t id = Settings::CreateCategory("OldName");
    Settings::RenameCategory(id, "NewName");

    const Settings::Category* cat = Settings::GetCategory(id);
    ASSERT_NE(cat, nullptr);
    EXPECT_STREQ(cat->name, "NewName");
}

// =============================================================================
// Title-Category Assignment Tests
// =============================================================================

TEST_F(SettingsTest, TitleHasCategory_UnassignedReturnsFalse) {
    uint16_t catId = Settings::CreateCategory("Games");
    EXPECT_FALSE(Settings::TitleHasCategory(0x0005000010145D00, catId));
}

TEST_F(SettingsTest, AssignTitleToCategory_Works) {
    uint16_t catId = Settings::CreateCategory("Games");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, catId);
    EXPECT_TRUE(Settings::TitleHasCategory(titleId, catId));
}

TEST_F(SettingsTest, AssignTitleToCategory_DuplicateIgnored) {
    uint16_t catId = Settings::CreateCategory("Games");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, catId);
    Settings::AssignTitleToCategory(titleId, catId);

    int count = 0;
    for (const auto& tc : Settings::Get().titleCategories) {
        if (tc.titleId == titleId && tc.categoryId == catId) count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(SettingsTest, AssignTitleToCategory_MultipleCategories) {
    uint16_t cat1 = Settings::CreateCategory("Action");
    uint16_t cat2 = Settings::CreateCategory("Multiplayer");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, cat1);
    Settings::AssignTitleToCategory(titleId, cat2);

    EXPECT_TRUE(Settings::TitleHasCategory(titleId, cat1));
    EXPECT_TRUE(Settings::TitleHasCategory(titleId, cat2));
}

TEST_F(SettingsTest, RemoveTitleFromCategory_Works) {
    uint16_t catId = Settings::CreateCategory("Games");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, catId);
    Settings::RemoveTitleFromCategory(titleId, catId);

    EXPECT_FALSE(Settings::TitleHasCategory(titleId, catId));
}

TEST_F(SettingsTest, RemoveTitleFromCategory_LeavesOtherAssignments) {
    uint16_t cat1 = Settings::CreateCategory("Action");
    uint16_t cat2 = Settings::CreateCategory("RPG");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, cat1);
    Settings::AssignTitleToCategory(titleId, cat2);
    Settings::RemoveTitleFromCategory(titleId, cat1);

    EXPECT_FALSE(Settings::TitleHasCategory(titleId, cat1));
    EXPECT_TRUE(Settings::TitleHasCategory(titleId, cat2));
}

TEST_F(SettingsTest, GetCategoriesForTitle_ReturnsCorrectCategories) {
    uint16_t cat1 = Settings::CreateCategory("Action");
    uint16_t cat2 = Settings::CreateCategory("Favorite");
    uint64_t titleId = 0x0005000010145D00;

    Settings::AssignTitleToCategory(titleId, cat1);
    Settings::AssignTitleToCategory(titleId, cat2);

    uint16_t outIds[16];
    int count = Settings::GetCategoriesForTitle(titleId, outIds, 16);
    EXPECT_EQ(count, 2);
}

TEST_F(SettingsTest, GetCategoriesForTitle_UnassignedReturnsZero) {
    uint16_t outIds[16];
    int count = Settings::GetCategoriesForTitle(0x0005000010145D00, outIds, 16);
    EXPECT_EQ(count, 0);
}

// =============================================================================
// Category Ordering Tests
// =============================================================================

TEST_F(SettingsTest, MoveCategoryUp_MovesCorrectly) {
    uint16_t id1 = Settings::CreateCategory("First");
    uint16_t id2 = Settings::CreateCategory("Second");

    Settings::MoveCategoryUp(id2);

    EXPECT_EQ(Settings::Get().categories[0].id, id2);
    EXPECT_EQ(Settings::Get().categories[1].id, id1);
}

TEST_F(SettingsTest, MoveCategoryUp_AtTopIsNoOp) {
    uint16_t id1 = Settings::CreateCategory("First");
    Settings::CreateCategory("Second");

    Settings::MoveCategoryUp(id1);
    EXPECT_EQ(Settings::Get().categories[0].id, id1);
}

TEST_F(SettingsTest, MoveCategoryDown_MovesCorrectly) {
    uint16_t id1 = Settings::CreateCategory("First");
    uint16_t id2 = Settings::CreateCategory("Second");

    Settings::MoveCategoryDown(id1);

    EXPECT_EQ(Settings::Get().categories[0].id, id2);
    EXPECT_EQ(Settings::Get().categories[1].id, id1);
}

TEST_F(SettingsTest, MoveCategoryDown_AtBottomIsNoOp) {
    Settings::CreateCategory("First");
    uint16_t id2 = Settings::CreateCategory("Second");

    Settings::MoveCategoryDown(id2);
    EXPECT_EQ(Settings::Get().categories[1].id, id2);
}

// =============================================================================
// Category Visibility Tests
// =============================================================================

TEST_F(SettingsTest, SetCategoryHidden_HidesCategory) {
    uint16_t id = Settings::CreateCategory("Hidden");
    EXPECT_FALSE(Settings::IsCategoryHidden(id));

    Settings::SetCategoryHidden(id, true);
    EXPECT_TRUE(Settings::IsCategoryHidden(id));
}

TEST_F(SettingsTest, SetCategoryHidden_ShowsCategory) {
    uint16_t id = Settings::CreateCategory("Visible");
    Settings::SetCategoryHidden(id, true);
    Settings::SetCategoryHidden(id, false);
    EXPECT_FALSE(Settings::IsCategoryHidden(id));
}

TEST_F(SettingsTest, GetSortedCategoryIndices_ReturnsCorrectOrder) {
    Settings::CreateCategory("Cat1");
    Settings::CreateCategory("Cat2");
    Settings::CreateCategory("Cat3");

    int indices[16];
    int count = Settings::GetSortedCategoryIndices(indices, 16);

    EXPECT_EQ(count, 3);
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
    EXPECT_EQ(indices[2], 2);
}

TEST_F(SettingsTest, GetSortedCategoryIndices_ExcludesHidden) {
    Settings::CreateCategory("Visible1");
    uint16_t hiddenId = Settings::CreateCategory("Hidden");
    Settings::CreateCategory("Visible2");

    Settings::SetCategoryHidden(hiddenId, true);

    int indices[16];
    int count = Settings::GetSortedCategoryIndices(indices, 16, false);
    EXPECT_EQ(count, 2);
}

TEST_F(SettingsTest, GetSortedCategoryIndices_IncludesHiddenWhenRequested) {
    Settings::CreateCategory("Visible1");
    uint16_t hiddenId = Settings::CreateCategory("Hidden");
    Settings::CreateCategory("Visible2");

    Settings::SetCategoryHidden(hiddenId, true);

    int indices[16];
    int count = Settings::GetSortedCategoryIndices(indices, 16, true);
    EXPECT_EQ(count, 3);
}

// =============================================================================
// Reset Tests
// =============================================================================

TEST_F(SettingsTest, ResetToDefaults_ClearsFavorites) {
    Settings::AddFavorite(0x0005000010145D00);
    Settings::ResetToDefaults();
    EXPECT_TRUE(Settings::Get().favorites.empty());
}

TEST_F(SettingsTest, ResetToDefaults_ClearsCategories) {
    Settings::CreateCategory("Test");
    Settings::ResetToDefaults();
    EXPECT_EQ(Settings::GetCategoryCount(), 0);
}

TEST_F(SettingsTest, ResetToDefaults_RestoresDefaultColors) {
    Settings::Get().bgColor = 0xFF0000FF;
    Settings::ResetToDefaults();
    EXPECT_EQ(Settings::Get().bgColor, Settings::DEFAULT_BG_COLOR);
}

// =============================================================================
// Get Function Tests
// =============================================================================

TEST_F(SettingsTest, Get_ReturnsReference) {
    Settings::Get().lastIndex = 42;
    EXPECT_EQ(Settings::Get().lastIndex, 42);
}

TEST_F(SettingsTest, Get_ModificationsArePersistent) {
    Settings::Get().showNumbers = true;
    EXPECT_TRUE(Settings::Get().showNumbers);
    Settings::Get().showNumbers = false;
    EXPECT_FALSE(Settings::Get().showNumbers);
}
