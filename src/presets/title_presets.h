/**
 * Title Presets System
 *
 * This module manages a collection of Wii U title metadata that can be used
 * to create preconfigured categories. The preset data includes publisher,
 * release date, genre, and region information for known titles.
 *
 * DATA SOURCE:
 * ------------
 * The preset data should be stored in a JSON file on the SD card:
 *   sd:/wiiu/plugins/config/TitleSwitcher_presets.json
 *
 * The recommended source is GameTDB (gametdb.com/WiiU) which provides:
 *   - Game IDs (product codes) that match the Wii U's ACPMetaXml.product_code
 *   - Publisher, developer, release dates, genres, ratings
 *   - Download: https://www.gametdb.com/wiiutdb.zip
 *
 * Use the included Python converter script to transform wiiutdb.xml to JSON:
 *   python3 tools/convert_gametdb.py wiiutdb.xml TitleSwitcher_presets.json
 *
 * JSON SCHEMA:
 * ------------
 * {
 *   "version": 1,
 *   "titles": [
 *     {
 *       "id": "ARDE01",                    // Required: GameTDB game ID (matches product code)
 *       "name": "Super Mario 3D World",    // Required: Display name
 *       "publisher": "Nintendo",           // Optional: Publisher name
 *       "developer": "Nintendo EAD Tokyo", // Optional: Developer name
 *       "releaseDate": "2013-11-22",       // Optional: YYYY-MM-DD or YYYY
 *       "region": "USA",                   // Optional: USA, EUR, JPN, etc.
 *       "genre": "Platformer"              // Optional: Game genre
 *     },
 *     ...
 *   ]
 * }
 *
 * Note: The "id" field is the GameTDB game ID which corresponds to the last
 * portion of the Wii U product code (e.g., "ARDE01" from "WUP-P-ARDE01").
 * This allows automatic matching against installed titles.
 *
 * USAGE:
 * ------
 *   // Load presets from SD card
 *   TitlePresets::Load();
 *
 *   // Get metadata for a title by its product code (from Titles::TitleInfo)
 *   const TitlePreset* preset = TitlePresets::GetPresetByGameId("ARDE01");
 *   if (preset) {
 *       printf("Publisher: %s\n", preset->publisher);
 *   }
 *
 *   // Get all unique publishers for category creation
 *   std::vector<const char*> publishers;
 *   TitlePresets::GetUniquePublishers(publishers);
 *
 *   // Get game IDs by publisher (can then match to installed titles)
 *   std::vector<const char*> nintendoGames;
 *   TitlePresets::GetGameIdsByPublisher("Nintendo", nintendoGames);
 */

#pragma once

#include <cstdint>
#include <vector>

namespace TitlePresets {

// =============================================================================
// Constants
// =============================================================================

// Maximum lengths for string fields
constexpr int MAX_GAME_ID = 16;        // GameTDB game ID (e.g., "ARDE01")
constexpr int MAX_PRESET_NAME = 128;
constexpr int MAX_PUBLISHER_NAME = 64;
constexpr int MAX_DEVELOPER_NAME = 64;
constexpr int MAX_REGION_NAME = 16;
constexpr int MAX_GENRE_NAME = 32;

// Maximum number of presets that can be loaded
constexpr int MAX_PRESETS = 2048;

// Current preset file version
constexpr int PRESET_VERSION = 1;

// =============================================================================
// Data Structures
// =============================================================================

/**
 * Metadata for a single Wii U title.
 *
 * Titles are identified by their GameTDB game ID (product code), which
 * can be matched against the productCode field in Titles::TitleInfo.
 */
struct TitlePreset {
    // GameTDB game ID / product code (e.g., "ARDE01", "ALZE01")
    // This matches against Titles::TitleInfo::productCode
    char gameId[MAX_GAME_ID];

    // Display name of the title
    char name[MAX_PRESET_NAME];

    // Publisher name (e.g., "Nintendo", "Capcom")
    char publisher[MAX_PUBLISHER_NAME];

    // Developer name (e.g., "Nintendo EAD", "Platinum Games")
    char developer[MAX_DEVELOPER_NAME];

    // Release year (0 if unknown)
    uint16_t releaseYear;

    // Release month (1-12, 0 if unknown)
    uint8_t releaseMonth;

    // Release day (1-31, 0 if unknown)
    uint8_t releaseDay;

    // Region code (e.g., "USA", "EUR", "JPN")
    char region[MAX_REGION_NAME];

    // Game genre (e.g., "Platformer", "RPG", "Action")
    char genre[MAX_GENRE_NAME];

    // Default constructor
    TitlePreset() : releaseYear(0), releaseMonth(0), releaseDay(0) {
        gameId[0] = '\0';
        name[0] = '\0';
        publisher[0] = '\0';
        developer[0] = '\0';
        region[0] = '\0';
        genre[0] = '\0';
    }
};

/**
 * Statistics about loaded presets.
 */
struct PresetStats {
    int totalPresets;
    int uniquePublishers;
    int uniqueDevelopers;
    int uniqueGenres;
    int uniqueRegions;
    int titlesWithReleaseDate;
};

// =============================================================================
// Core Functions
// =============================================================================

/**
 * Load presets from the JSON file on SD card.
 *
 * Looks for: sd:/wiiu/plugins/config/TitleSwitcher_presets.json
 *
 * @return true if loaded successfully, false if file not found or parse error
 */
bool Load();

/**
 * Check if presets have been loaded.
 *
 * @return true if Load() was called successfully
 */
bool IsLoaded();

/**
 * Get the number of loaded presets.
 *
 * @return Number of title presets currently loaded
 */
int GetPresetCount();

/**
 * Get statistics about loaded presets.
 *
 * @param stats Output structure to fill with statistics
 */
void GetStats(PresetStats& stats);

/**
 * Get preset data for a title by its game ID (product code).
 *
 * This performs partial matching to support both full product codes
 * (e.g., "WUP-P-ARDE01") and GameTDB-style IDs (e.g., "ARDE01").
 *
 * @param gameId The game ID or product code to look up
 * @return Pointer to preset data, or nullptr if not found
 */
const TitlePreset* GetPresetByGameId(const char* gameId);

/**
 * Get preset data by index (for iteration).
 *
 * @param index Index from 0 to GetPresetCount()-1
 * @return Pointer to preset data, or nullptr if out of range
 */
const TitlePreset* GetPresetByIndex(int index);

// =============================================================================
// Query Functions - For Building Categories
// =============================================================================

/**
 * Get a list of all unique publisher names.
 *
 * @param outPublishers Vector to receive publisher names (cleared first)
 * @return Number of unique publishers
 */
int GetUniquePublishers(std::vector<const char*>& outPublishers);

/**
 * Get a list of all unique developer names.
 *
 * @param outDevelopers Vector to receive developer names (cleared first)
 * @return Number of unique developers
 */
int GetUniqueDevelopers(std::vector<const char*>& outDevelopers);

/**
 * Get a list of all unique genres.
 *
 * @param outGenres Vector to receive genre names (cleared first)
 * @return Number of unique genres
 */
int GetUniqueGenres(std::vector<const char*>& outGenres);

/**
 * Get a list of all unique regions.
 *
 * @param outRegions Vector to receive region codes (cleared first)
 * @return Number of unique regions
 */
int GetUniqueRegions(std::vector<const char*>& outRegions);

/**
 * Get a list of all release years present in the data.
 *
 * @param outYears Vector to receive years (cleared first)
 * @return Number of unique years
 */
int GetUniqueYears(std::vector<uint16_t>& outYears);

/**
 * Get all game IDs from a specific publisher.
 *
 * @param publisher Publisher name to match (case-insensitive)
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByPublisher(const char* publisher, std::vector<const char*>& outGameIds);

/**
 * Get all game IDs from a specific developer.
 *
 * @param developer Developer name to match (case-insensitive)
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByDeveloper(const char* developer, std::vector<const char*>& outGameIds);

/**
 * Get all game IDs of a specific genre.
 *
 * @param genre Genre name to match (case-insensitive)
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByGenre(const char* genre, std::vector<const char*>& outGameIds);

/**
 * Get all game IDs from a specific region.
 *
 * @param region Region code to match (e.g., "USA", "EUR", "JPN")
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByRegion(const char* region, std::vector<const char*>& outGameIds);

/**
 * Get all game IDs released in a specific year.
 *
 * @param year Release year to match
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByYear(uint16_t year, std::vector<const char*>& outGameIds);

/**
 * Get all game IDs released in a year range.
 *
 * @param startYear Start of range (inclusive)
 * @param endYear End of range (inclusive)
 * @param outGameIds Vector to receive matching game IDs (cleared first)
 * @return Number of matching titles
 */
int GetGameIdsByYearRange(uint16_t startYear, uint16_t endYear, std::vector<const char*>& outGameIds);

// =============================================================================
// Category Generation Helpers
// =============================================================================

/**
 * Category preset types that can be auto-generated.
 */
enum class PresetCategoryType {
    Publisher,      // One category per publisher
    Developer,      // One category per developer
    Genre,          // One category per genre
    Region,         // One category per region
    ReleaseYear,    // One category per release year
    ReleasePeriod   // Categories like "2012-2014", "2015-2017"
};

/**
 * Information about a suggested category.
 */
struct SuggestedCategory {
    char name[MAX_PUBLISHER_NAME];  // Category name
    int titleCount;                  // Number of titles that would be in this category
};

/**
 * Get suggested categories of a specific type.
 *
 * This helps users see what categories could be created from the preset data.
 *
 * @param type Type of categories to suggest
 * @param outSuggestions Vector to receive suggestions (cleared first)
 * @param minTitles Minimum number of titles for a category to be suggested (default: 1)
 * @return Number of suggestions
 */
int GetSuggestedCategories(PresetCategoryType type,
                           std::vector<SuggestedCategory>& outSuggestions,
                           int minTitles = 1);

} // namespace TitlePresets
