/**
 * Layout Measurements System
 *
 * Centralizes all layout-related measurements, spacing, and sizing constants.
 * This provides a single source of truth for UI layout calculations, making it
 * easy to adjust spacing, resize elements, or adapt to different screen sizes.
 *
 * USAGE:
 * ------
 *   const auto& m = Measurements::Get();
 *   int iconX = m.GetIconPixelX();
 *   int infoRow = m.GetInfoStartRow();
 *
 * DESIGN:
 * -------
 * Currently returns a single Measurements instance suitable for both DRC and TV
 * (since OSScreen renders identically to both). The architecture supports future
 * per-screen measurements via GetMeasurements(ScreenType) if needed.
 */

#pragma once

#include <cstdint>

namespace Measurements {

// =============================================================================
// Icon Sizing
// =============================================================================

/// Icon display size in pixels (square)
constexpr int ICON_SIZE = 128;

/// Margin from panel edge to icon (in pixels)
constexpr int ICON_MARGIN_PX = 170;

/// Row where icon is positioned (grid row, converted to pixels for drawing)
constexpr int ICON_ROW = 4;

// =============================================================================
// Row Offsets (relative to LIST_START_ROW)
// =============================================================================
// These define semantic positions within the content area.
// LIST_START_ROW is defined in menu.h as row 2.

/// Offset for title underline/separator
constexpr int ROW_OFFSET_UNDERLINE = 1;

/// Offset for first content item after header
constexpr int ROW_OFFSET_CONTENT_START = 2;

/// Offset for first list item or section
constexpr int ROW_OFFSET_SECTION_START = 3;

/// Offset for second line of content
constexpr int ROW_OFFSET_CONTENT_LINE2 = 4;

/// Offset for spacing/gap line
constexpr int ROW_OFFSET_GAP = 5;

/// Offset for hint text or secondary section
constexpr int ROW_OFFSET_HINT = 6;

/// Offset for info section start (after icon area)
constexpr int ROW_OFFSET_INFO_START = 7;

/// Offset for additional info lines
constexpr int ROW_OFFSET_INFO_LINE2 = 8;
constexpr int ROW_OFFSET_INFO_LINE3 = 9;

/// Offset for action buttons/hints at bottom of panel
constexpr int ROW_OFFSET_ACTIONS = 11;

// =============================================================================
// List Configuration
// =============================================================================

/// Visible rows in category edit mode (checkbox list)
constexpr int CATEGORY_EDIT_VISIBLE_ROWS = 10;

/// Visible rows in category management mode
constexpr int CATEGORY_MANAGE_VISIBLE_ROWS = 10;

/// Maximum characters for category bar before truncation
constexpr int CATEGORY_BAR_MAX_WIDTH = 60;

// =============================================================================
// Indentation
// =============================================================================

/// Column indent for sub-items (e.g., category list items)
constexpr int INDENT_SUB_ITEM = 2;

constexpr const char* HEADER_DIVIDER = "------------------------------------------------------------";
constexpr const char* SECTION_UNDERLINE = "------------";
constexpr const char* SECTION_UNDERLINE_SHORT = "--------";

// =============================================================================
// Pixel Calculation Helpers
// =============================================================================

/**
 * Calculate the pixel X position for icon placement.
 *
 * @param detailsPanelCol The column where details panel starts
 * @return X pixel coordinate for icon
 */
int GetIconPixelX(int detailsPanelCol);

/**
 * Calculate the pixel Y position for icon placement.
 *
 * @return Y pixel coordinate for icon
 */
int GetIconPixelY();

/**
 * Get the info section start row (absolute row, not offset).
 *
 * @param listStartRow The row where content lists begin
 * @return Absolute row number for info section
 */
inline constexpr int GetInfoStartRow(int listStartRow)
{
    return listStartRow + ROW_OFFSET_INFO_START;
}

/**
 * Get the content start row (absolute row, not offset).
 *
 * @param listStartRow The row where content lists begin
 * @return Absolute row number for content start
 */
inline constexpr int GetContentStartRow(int listStartRow)
{
    return listStartRow + ROW_OFFSET_CONTENT_START;
}

/**
 * Get the section start row (absolute row, not offset).
 *
 * @param listStartRow The row where content lists begin
 * @return Absolute row number for section start
 */
inline constexpr int GetSectionStartRow(int listStartRow)
{
    return listStartRow + ROW_OFFSET_SECTION_START;
}

} // namespace Measurements
