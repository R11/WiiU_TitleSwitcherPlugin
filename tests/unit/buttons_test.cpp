/**
 * Unit tests for src/input/buttons.h
 *
 * Tests button combo detection and helper functions.
 * These are pure bitwise operations with zero dependencies.
 */

#include <gtest/gtest.h>
#include <cstring>
#include "input/buttons.h"

// =============================================================================
// IsComboPressed Tests
// =============================================================================

TEST(ButtonsTest, IsComboPressed_AllButtonsHeld_ReturnsTrue) {
    uint32_t combo = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS;
    uint32_t held = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS;
    EXPECT_TRUE(Buttons::IsComboPressed(held, combo));
}

TEST(ButtonsTest, IsComboPressed_PartialButtons_ReturnsFalse) {
    uint32_t combo = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_MINUS;
    uint32_t held = VPAD_BUTTON_L | VPAD_BUTTON_R;  // Missing MINUS
    EXPECT_FALSE(Buttons::IsComboPressed(held, combo));
}

TEST(ButtonsTest, IsComboPressed_ExtraButtons_ReturnsTrue) {
    uint32_t combo = VPAD_BUTTON_L | VPAD_BUTTON_R;
    uint32_t held = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_A;
    EXPECT_TRUE(Buttons::IsComboPressed(held, combo));
}

TEST(ButtonsTest, IsComboPressed_NoButtonsHeld_ReturnsFalse) {
    uint32_t combo = VPAD_BUTTON_L | VPAD_BUTTON_R;
    uint32_t held = 0;
    EXPECT_FALSE(Buttons::IsComboPressed(held, combo));
}

TEST(ButtonsTest, IsComboPressed_SingleButton_Works) {
    uint32_t combo = VPAD_BUTTON_A;
    EXPECT_TRUE(Buttons::IsComboPressed(VPAD_BUTTON_A, combo));
    EXPECT_FALSE(Buttons::IsComboPressed(VPAD_BUTTON_B, combo));
}

TEST(ButtonsTest, IsComboPressed_EmptyCombo_AlwaysTrue) {
    uint32_t combo = 0;
    EXPECT_TRUE(Buttons::IsComboPressed(VPAD_BUTTON_A, combo));
    EXPECT_TRUE(Buttons::IsComboPressed(0, combo));
}

// =============================================================================
// WasPressed Tests
// =============================================================================

TEST(ButtonsTest, WasPressed_ButtonPressed_ReturnsTrue) {
    uint32_t triggered = VPAD_BUTTON_A;
    EXPECT_TRUE(Buttons::WasPressed(triggered, VPAD_BUTTON_A));
}

TEST(ButtonsTest, WasPressed_ButtonNotPressed_ReturnsFalse) {
    uint32_t triggered = VPAD_BUTTON_B;
    EXPECT_FALSE(Buttons::WasPressed(triggered, VPAD_BUTTON_A));
}

TEST(ButtonsTest, WasPressed_MultiplePressed_FindsEach) {
    uint32_t triggered = VPAD_BUTTON_A | VPAD_BUTTON_B | VPAD_BUTTON_X;
    EXPECT_TRUE(Buttons::WasPressed(triggered, VPAD_BUTTON_A));
    EXPECT_TRUE(Buttons::WasPressed(triggered, VPAD_BUTTON_B));
    EXPECT_TRUE(Buttons::WasPressed(triggered, VPAD_BUTTON_X));
    EXPECT_FALSE(Buttons::WasPressed(triggered, VPAD_BUTTON_Y));
}

TEST(ButtonsTest, WasPressed_NoButtonsPressed_ReturnsFalse) {
    EXPECT_FALSE(Buttons::WasPressed(0, VPAD_BUTTON_A));
}

// =============================================================================
// IsHeld Tests
// =============================================================================

TEST(ButtonsTest, IsHeld_ButtonHeld_ReturnsTrue) {
    uint32_t held = VPAD_BUTTON_ZL;
    EXPECT_TRUE(Buttons::IsHeld(held, VPAD_BUTTON_ZL));
}

TEST(ButtonsTest, IsHeld_ButtonNotHeld_ReturnsFalse) {
    uint32_t held = VPAD_BUTTON_ZR;
    EXPECT_FALSE(Buttons::IsHeld(held, VPAD_BUTTON_ZL));
}

TEST(ButtonsTest, IsHeld_MultipleHeld_FindsEach) {
    uint32_t held = VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_ZL | VPAD_BUTTON_ZR;
    EXPECT_TRUE(Buttons::IsHeld(held, VPAD_BUTTON_L));
    EXPECT_TRUE(Buttons::IsHeld(held, VPAD_BUTTON_R));
    EXPECT_TRUE(Buttons::IsHeld(held, VPAD_BUTTON_ZL));
    EXPECT_TRUE(Buttons::IsHeld(held, VPAD_BUTTON_ZR));
    EXPECT_FALSE(Buttons::IsHeld(held, VPAD_BUTTON_A));
}

// =============================================================================
// Button Struct Tests
// =============================================================================

TEST(ButtonsTest, Button_Pressed_Works) {
    Buttons::Button confirm = { VPAD_BUTTON_A, "A" };
    EXPECT_TRUE(confirm.Pressed(VPAD_BUTTON_A));
    EXPECT_TRUE(confirm.Pressed(VPAD_BUTTON_A | VPAD_BUTTON_B));
    EXPECT_FALSE(confirm.Pressed(VPAD_BUTTON_B));
    EXPECT_FALSE(confirm.Pressed(0));
}

TEST(ButtonsTest, Button_Held_Works) {
    Buttons::Button cancel = { VPAD_BUTTON_B, "B" };
    EXPECT_TRUE(cancel.Held(VPAD_BUTTON_B));
    EXPECT_TRUE(cancel.Held(VPAD_BUTTON_A | VPAD_BUTTON_B));
    EXPECT_FALSE(cancel.Held(VPAD_BUTTON_A));
    EXPECT_FALSE(cancel.Held(0));
}

TEST(ButtonsTest, Button_Label_IsCorrect) {
    Buttons::Button testBtn = { VPAD_BUTTON_X, "X" };
    EXPECT_STREQ(testBtn.label, "X");
}

// =============================================================================
// Menu Open Combo Constant Tests
// =============================================================================

TEST(ButtonsTest, MenuOpenCombo_ContainsExpectedButtons) {
    uint32_t combo = Buttons::Actions::MENU_OPEN_COMBO;
    EXPECT_TRUE((combo & VPAD_BUTTON_L) != 0);
    EXPECT_TRUE((combo & VPAD_BUTTON_MINUS) != 0);
    EXPECT_TRUE((combo & VPAD_BUTTON_RIGHT) != 0);
}

TEST(ButtonsTest, MenuOpenCombo_ExactMatch) {
    uint32_t expected = VPAD_BUTTON_L | VPAD_BUTTON_MINUS | VPAD_BUTTON_RIGHT;
    EXPECT_EQ(Buttons::Actions::MENU_OPEN_COMBO, expected);
}

TEST(ButtonsTest, MenuOpenCombo_DetectedCorrectly) {
    uint32_t combo = Buttons::Actions::MENU_OPEN_COMBO;
    uint32_t held = VPAD_BUTTON_L | VPAD_BUTTON_MINUS | VPAD_BUTTON_RIGHT;
    EXPECT_TRUE(Buttons::IsComboPressed(held, combo));

    held = VPAD_BUTTON_L | VPAD_BUTTON_MINUS;
    EXPECT_FALSE(Buttons::IsComboPressed(held, combo));
}

// =============================================================================
// Action Button Tests
// =============================================================================

TEST(ButtonsTest, Actions_Confirm_IsButtonA) {
    EXPECT_EQ(Buttons::Actions::CONFIRM.input, VPAD_BUTTON_A);
    EXPECT_STREQ(Buttons::Actions::CONFIRM.label, "A");
}

TEST(ButtonsTest, Actions_Cancel_IsButtonB) {
    EXPECT_EQ(Buttons::Actions::CANCEL.input, VPAD_BUTTON_B);
    EXPECT_STREQ(Buttons::Actions::CANCEL.label, "B");
}

TEST(ButtonsTest, Actions_CategoryNavigation_UsesZLZR) {
    EXPECT_EQ(Buttons::Actions::CATEGORY_PREV.input, VPAD_BUTTON_ZL);
    EXPECT_EQ(Buttons::Actions::CATEGORY_NEXT.input, VPAD_BUTTON_ZR);
}

// =============================================================================
// Skip Constants Tests
// =============================================================================

TEST(ButtonsTest, Skip_SmallIsReasonable) {
    EXPECT_GT(Buttons::Skip::SMALL, 0);
    EXPECT_LT(Buttons::Skip::SMALL, 20);
}

TEST(ButtonsTest, Skip_LargeLargerThanSmall) {
    EXPECT_GT(Buttons::Skip::LARGE, Buttons::Skip::SMALL);
}
