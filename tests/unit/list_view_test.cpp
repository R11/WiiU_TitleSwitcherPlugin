/**
 * Unit tests for UI::ListView component
 */

#include <gtest/gtest.h>
#include "ui/list_view.h"

using namespace UI::ListView;

// =============================================================================
// State Tests
// =============================================================================

class ListViewStateTest : public ::testing::Test {
protected:
    State state;

    void SetUp() override {
        state = State();
    }
};

TEST_F(ListViewStateTest, DefaultState_IsZeroInitialized) {
    EXPECT_EQ(state.selectedIndex, 0);
    EXPECT_EQ(state.scrollOffset, 0);
    EXPECT_EQ(state.itemCount, 0);
}

TEST_F(ListViewStateTest, SetItemCount_UpdatesCount) {
    state.SetItemCount(10, 5);
    EXPECT_EQ(state.itemCount, 10);
}

TEST_F(ListViewStateTest, SetItemCount_ClampsSelection) {
    state.selectedIndex = 15;
    state.SetItemCount(10, 5);
    EXPECT_EQ(state.selectedIndex, 9);  // Clamped to last item
}

TEST_F(ListViewStateTest, SetItemCount_ClampsScroll) {
    // Scroll follows selection - when selection is 0, scroll should be 0
    // even if scroll was previously set higher
    state.scrollOffset = 20;
    state.SetItemCount(10, 5);
    // Selection is 0 by default, so scroll should be 0 to keep it visible
    EXPECT_EQ(state.scrollOffset, 0);
}

TEST_F(ListViewStateTest, SetItemCount_ClampsScrollWithSelection) {
    // When selection is near the end, scroll should clamp to max valid value
    state.selectedIndex = 9;
    state.scrollOffset = 20;
    state.SetItemCount(10, 5);
    // Max scroll = 10 - 5 = 5, selection 9 needs scroll of at least 5
    EXPECT_EQ(state.scrollOffset, 5);
}

TEST_F(ListViewStateTest, SetItemCount_EmptyList) {
    state.selectedIndex = 5;
    state.scrollOffset = 3;
    state.SetItemCount(0, 5);
    EXPECT_EQ(state.selectedIndex, 0);
    EXPECT_EQ(state.scrollOffset, 0);
}

TEST_F(ListViewStateTest, Clamp_SelectionAboveList) {
    state.selectedIndex = -5;
    state.itemCount = 10;
    state.Clamp(5);
    EXPECT_EQ(state.selectedIndex, 0);
}

TEST_F(ListViewStateTest, Clamp_SelectionBelowList) {
    state.selectedIndex = 15;
    state.itemCount = 10;
    state.Clamp(5);
    EXPECT_EQ(state.selectedIndex, 9);
}

TEST_F(ListViewStateTest, Clamp_ScrollFollowsSelection) {
    state.selectedIndex = 8;
    state.scrollOffset = 0;
    state.itemCount = 10;
    state.Clamp(5);
    // Selection 8 should be visible, scroll should adjust
    EXPECT_GE(state.selectedIndex, state.scrollOffset);
    EXPECT_LT(state.selectedIndex, state.scrollOffset + 5);
}

TEST_F(ListViewStateTest, MoveSelection_Down) {
    state.itemCount = 10;
    state.MoveSelection(1, 5, false);
    EXPECT_EQ(state.selectedIndex, 1);
}

TEST_F(ListViewStateTest, MoveSelection_Up) {
    state.selectedIndex = 5;
    state.itemCount = 10;
    state.MoveSelection(-1, 5, false);
    EXPECT_EQ(state.selectedIndex, 4);
}

TEST_F(ListViewStateTest, MoveSelection_LargeSkip) {
    state.itemCount = 20;
    state.MoveSelection(10, 5, false);
    EXPECT_EQ(state.selectedIndex, 10);
}

TEST_F(ListViewStateTest, MoveSelection_ClampsAtBottom) {
    state.selectedIndex = 8;
    state.itemCount = 10;
    state.MoveSelection(5, 5, false);
    EXPECT_EQ(state.selectedIndex, 9);  // Clamped to last item
}

TEST_F(ListViewStateTest, MoveSelection_ClampsAtTop) {
    state.selectedIndex = 2;
    state.itemCount = 10;
    state.MoveSelection(-5, 5, false);
    EXPECT_EQ(state.selectedIndex, 0);  // Clamped to first item
}

TEST_F(ListViewStateTest, MoveSelection_WrapFromBottom) {
    state.selectedIndex = 9;
    state.itemCount = 10;
    state.MoveSelection(1, 5, true);  // Wrap enabled
    EXPECT_EQ(state.selectedIndex, 0);  // Wrapped to top
}

TEST_F(ListViewStateTest, MoveSelection_WrapFromTop) {
    state.selectedIndex = 0;
    state.itemCount = 10;
    state.MoveSelection(-1, 5, true);  // Wrap enabled
    EXPECT_EQ(state.selectedIndex, 9);  // Wrapped to bottom
}

TEST_F(ListViewStateTest, MoveSelection_NoWrapFromBottom) {
    state.selectedIndex = 9;
    state.itemCount = 10;
    state.MoveSelection(1, 5, false);  // Wrap disabled
    EXPECT_EQ(state.selectedIndex, 9);  // Stays at bottom
}

TEST_F(ListViewStateTest, MoveSelection_EmptyList) {
    state.itemCount = 0;
    state.MoveSelection(1, 5, false);
    EXPECT_EQ(state.selectedIndex, 0);  // No change
}

TEST_F(ListViewStateTest, MoveSelection_ScrollAdjusts) {
    state.itemCount = 20;
    state.scrollOffset = 0;
    state.MoveSelection(15, 5, false);  // Move to item 15
    EXPECT_EQ(state.selectedIndex, 15);
    // Selection should be visible
    EXPECT_LE(state.scrollOffset, 15);
    EXPECT_GT(state.scrollOffset + 5, 15);
}

// =============================================================================
// Utility Function Tests
// =============================================================================

class ListViewUtilTest : public ::testing::Test {
protected:
    State state;
    Config config;

    void SetUp() override {
        state = State();
        config = Config();
        config.visibleRows = 5;
    }
};

TEST_F(ListViewUtilTest, IsScrollable_SmallList_ReturnsFalse) {
    state.itemCount = 3;
    EXPECT_FALSE(IsScrollable(state, config));
}

TEST_F(ListViewUtilTest, IsScrollable_ExactFit_ReturnsFalse) {
    state.itemCount = 5;
    EXPECT_FALSE(IsScrollable(state, config));
}

TEST_F(ListViewUtilTest, IsScrollable_LargeList_ReturnsTrue) {
    state.itemCount = 10;
    EXPECT_TRUE(IsScrollable(state, config));
}

TEST_F(ListViewUtilTest, CanScrollUp_AtTop_ReturnsFalse) {
    state.scrollOffset = 0;
    EXPECT_FALSE(CanScrollUp(state));
}

TEST_F(ListViewUtilTest, CanScrollUp_NotAtTop_ReturnsTrue) {
    state.scrollOffset = 3;
    EXPECT_TRUE(CanScrollUp(state));
}

TEST_F(ListViewUtilTest, CanScrollDown_AtBottom_ReturnsFalse) {
    state.itemCount = 10;
    state.scrollOffset = 5;  // 10 - 5 = 5
    EXPECT_FALSE(CanScrollDown(state, config));
}

TEST_F(ListViewUtilTest, CanScrollDown_NotAtBottom_ReturnsTrue) {
    state.itemCount = 10;
    state.scrollOffset = 0;
    EXPECT_TRUE(CanScrollDown(state, config));
}

TEST_F(ListViewUtilTest, GetSelectedIndex_EmptyList_ReturnsNegOne) {
    state.itemCount = 0;
    EXPECT_EQ(GetSelectedIndex(state), -1);
}

TEST_F(ListViewUtilTest, GetSelectedIndex_NonEmpty_ReturnsIndex) {
    state.itemCount = 10;
    state.selectedIndex = 5;
    EXPECT_EQ(GetSelectedIndex(state), 5);
}

// =============================================================================
// Config Tests
// =============================================================================

TEST(ListViewConfigTest, DefaultConfig_HasReasonableDefaults) {
    Config config;
    EXPECT_EQ(config.col, 0);
    EXPECT_EQ(config.row, 0);
    EXPECT_EQ(config.width, 30);
    EXPECT_EQ(config.visibleRows, 10);
    EXPECT_FALSE(config.showLineNumbers);
    EXPECT_TRUE(config.showScrollIndicators);
    EXPECT_FALSE(config.wrapAround);
    EXPECT_TRUE(config.canConfirm);
    EXPECT_TRUE(config.canCancel);
    EXPECT_FALSE(config.canReorder);
    EXPECT_FALSE(config.canDelete);
    EXPECT_FALSE(config.canToggle);
    EXPECT_FALSE(config.canFavorite);
}

// =============================================================================
// ItemView Tests
// =============================================================================

TEST(ListViewItemViewTest, DefaultItemView_HasReasonableDefaults) {
    ItemView view;
    EXPECT_STREQ(view.text, "");
    EXPECT_STREQ(view.prefix, "  ");
    EXPECT_STREQ(view.suffix, "");
    EXPECT_EQ(view.textColor, 0xFFFFFFFF);
    EXPECT_EQ(view.prefixColor, 0xFFFFFFFF);
    EXPECT_FALSE(view.dimmed);
}
