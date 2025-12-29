/**
 * Details View System
 *
 * Provides a detailed game information display for the secondary screen.
 * Shows large artwork, title name, and metadata from GameTDB presets.
 *
 * USAGE:
 * ------
 *   Screen::Descriptor screen = Screen::GetDescriptor(Screen::Target::TV);
 *   DetailsView::Layout layout = DetailsView::ComputeLayout(screen);
 *
 *   // Get details for selected title
 *   const TitleInfo* title = Titles::GetTitle(selectedIndex);
 *   DetailsView::GameDetails details = DetailsView::BuildGameDetails(title);
 *
 *   // Render using layout and details
 */

#pragma once

#include <cstdint>
#include "screen.h"

namespace DetailsView {

/**
 * Game Details
 *
 * Aggregated information about a game for display.
 * Built from TitleInfo and optional TitlePreset data.
 */
struct GameDetails {
    const char* title;          // Game title name
    const char* publisher;      // Publisher (from preset)
    const char* developer;      // Developer (from preset)
    const char* genre;          // Genre (from preset)
    const char* region;         // Region code (from preset)
    uint64_t titleId;           // Title ID for icon loading
    uint16_t releaseYear;       // Release year (0 if unknown)
    uint8_t releaseMonth;       // Release month (0 if unknown)
    uint8_t releaseDay;         // Release day (0 if unknown)
    bool hasPresetData;         // Whether GameTDB data is available

    GameDetails() :
        title(nullptr),
        publisher(nullptr),
        developer(nullptr),
        genre(nullptr),
        region(nullptr),
        titleId(0),
        releaseYear(0),
        releaseMonth(0),
        releaseDay(0),
        hasPresetData(false)
    {}
};

/**
 * Details Layout
 *
 * Computed layout parameters for rendering the details view.
 */
struct Layout {
    // Artwork area (left side)
    struct {
        int x;
        int y;
        int size;       // Square dimension
    } artwork;

    // Title text area
    struct {
        int x;
        int y;
        int fontSize;
        int maxWidth;
    } title;

    // Divider line under title
    struct {
        int x;
        int y;
        int width;
    } divider;

    // Info rows (metadata)
    struct {
        int x;              // Left edge of info section
        int y;              // Top of first info row
        int lineHeight;     // Height per row
        int labelWidth;     // Width reserved for labels
        int valueX;         // X position for values
        int maxRows;        // Maximum rows that fit
    } info;

    // Button hints at bottom
    struct {
        int y;
        int fontSize;
    } hints;

    // Content area bounds
    int contentX;
    int contentY;
    int contentWidth;
    int contentHeight;
};

/**
 * Compute details layout for a screen.
 *
 * @param screen Screen descriptor
 * @return Computed layout
 */
Layout ComputeLayout(const Screen::Descriptor& screen);

/**
 * Info row labels for display
 */
constexpr const char* INFO_LABEL_PUBLISHER = "Publisher";
constexpr const char* INFO_LABEL_DEVELOPER = "Developer";
constexpr const char* INFO_LABEL_RELEASED = "Released";
constexpr const char* INFO_LABEL_GENRE = "Genre";
constexpr const char* INFO_LABEL_REGION = "Region";
constexpr const char* INFO_LABEL_TITLE_ID = "Title ID";
constexpr const char* INFO_LABEL_PRODUCT = "Product";

} // namespace DetailsView
