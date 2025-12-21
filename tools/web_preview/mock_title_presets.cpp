/**
 * Mock TitlePresets for Web Preview
 *
 * Provides stub implementations - no preset metadata in web preview.
 */

#include "presets/title_presets.h"

namespace TitlePresets {

static bool sLoaded = false;

bool Load() {
    sLoaded = true;
    return true;
}

bool IsLoaded() {
    return sLoaded;
}

int GetPresetCount() {
    return 0;
}

void GetStats(PresetStats& stats) {
    stats.totalPresets = 0;
    stats.uniquePublishers = 0;
    stats.uniqueDevelopers = 0;
    stats.uniqueGenres = 0;
    stats.uniqueRegions = 0;
    stats.titlesWithReleaseDate = 0;
}

const TitlePreset* GetPresetByGameId(const char* gameId) {
    (void)gameId;
    return nullptr;
}

const TitlePreset* GetPresetByIndex(int index) {
    (void)index;
    return nullptr;
}

int GetUniquePublishers(std::vector<const char*>& outPublishers) {
    outPublishers.clear();
    return 0;
}

int GetUniqueDevelopers(std::vector<const char*>& outDevelopers) {
    outDevelopers.clear();
    return 0;
}

int GetUniqueGenres(std::vector<const char*>& outGenres) {
    outGenres.clear();
    return 0;
}

int GetUniqueRegions(std::vector<const char*>& outRegions) {
    outRegions.clear();
    return 0;
}

int GetUniqueYears(std::vector<uint16_t>& outYears) {
    outYears.clear();
    return 0;
}

int GetGameIdsByPublisher(const char* publisher, std::vector<const char*>& outGameIds) {
    (void)publisher;
    outGameIds.clear();
    return 0;
}

int GetGameIdsByDeveloper(const char* developer, std::vector<const char*>& outGameIds) {
    (void)developer;
    outGameIds.clear();
    return 0;
}

int GetGameIdsByGenre(const char* genre, std::vector<const char*>& outGameIds) {
    (void)genre;
    outGameIds.clear();
    return 0;
}

int GetGameIdsByRegion(const char* region, std::vector<const char*>& outGameIds) {
    (void)region;
    outGameIds.clear();
    return 0;
}

int GetGameIdsByYear(uint16_t year, std::vector<const char*>& outGameIds) {
    (void)year;
    outGameIds.clear();
    return 0;
}

int GetGameIdsByYearRange(uint16_t startYear, uint16_t endYear, std::vector<const char*>& outGameIds) {
    (void)startYear;
    (void)endYear;
    outGameIds.clear();
    return 0;
}

int GetSuggestedCategories(PresetCategoryType type,
                           std::vector<SuggestedCategory>& outSuggestions,
                           int minTitles) {
    (void)type;
    (void)minTitles;
    outSuggestions.clear();
    return 0;
}

} // namespace TitlePresets
