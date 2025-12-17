/**
 * Title Presets Implementation
 *
 * See title_presets.h for usage documentation.
 *
 * This file implements a simple JSON parser for loading title preset data
 * from the SD card. The parser is minimal and handles the specific JSON
 * schema used by the presets file.
 */

#include "title_presets.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <set>
#include <string>

namespace TitlePresets {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Array of loaded presets
TitlePreset gPresets[MAX_PRESETS];
int gPresetCount = 0;
bool gIsLoaded = false;

// Path to the presets file on SD card
constexpr const char* PRESETS_FILE_PATH = "fs:/vol/external01/wiiu/plugins/config/TitleSwitcher_presets.json";

// =============================================================================
// Simple JSON Parser
// =============================================================================

/**
 * Skip whitespace in JSON string.
 */
const char* SkipWhitespace(const char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * Parse a JSON string value (assumes cursor is at opening quote).
 * Returns pointer after closing quote, or nullptr on error.
 */
const char* ParseString(const char* p, char* outStr, int maxLen) {
    if (*p != '"') return nullptr;
    p++; // Skip opening quote

    int i = 0;
    while (*p && *p != '"' && i < maxLen - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++; // Skip backslash
            switch (*p) {
                case 'n': outStr[i++] = '\n'; break;
                case 't': outStr[i++] = '\t'; break;
                case 'r': outStr[i++] = '\r'; break;
                case '"': outStr[i++] = '"'; break;
                case '\\': outStr[i++] = '\\'; break;
                default: outStr[i++] = *p; break;
            }
        } else {
            outStr[i++] = *p;
        }
        p++;
    }
    outStr[i] = '\0';

    if (*p != '"') return nullptr;
    return p + 1; // Skip closing quote
}

/**
 * Parse a date string (YYYY-MM-DD or YYYY) into components.
 */
void ParseDate(const char* dateStr, uint16_t* year, uint8_t* month, uint8_t* day) {
    *year = 0;
    *month = 0;
    *day = 0;

    int len = strlen(dateStr);

    // Parse year
    if (len >= 4) {
        *year = (dateStr[0] - '0') * 1000 +
                (dateStr[1] - '0') * 100 +
                (dateStr[2] - '0') * 10 +
                (dateStr[3] - '0');
    }

    // Parse month if present (YYYY-MM-DD format)
    if (len >= 7 && dateStr[4] == '-') {
        *month = (dateStr[5] - '0') * 10 + (dateStr[6] - '0');
    }

    // Parse day if present
    if (len >= 10 && dateStr[7] == '-') {
        *day = (dateStr[8] - '0') * 10 + (dateStr[9] - '0');
    }
}

/**
 * Find a key in a JSON object. Returns pointer to value after colon.
 */
const char* FindKey(const char* json, const char* key) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);

    const char* found = strstr(json, searchKey);
    if (!found) return nullptr;

    // Move past the key and find the colon
    found += strlen(searchKey);
    found = SkipWhitespace(found);
    if (*found != ':') return nullptr;
    found++;
    return SkipWhitespace(found);
}

/**
 * Find the end of a JSON value (string, number, object, or array).
 */
const char* SkipValue(const char* p) {
    p = SkipWhitespace(p);

    if (*p == '"') {
        // String
        p++;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) p++;
            p++;
        }
        if (*p == '"') p++;
    } else if (*p == '{') {
        // Object
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) p++;
                    p++;
                }
            }
            if (*p) p++;
        }
    } else if (*p == '[') {
        // Array
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) p++;
                    p++;
                }
            }
            if (*p) p++;
        }
    } else {
        // Number, true, false, null
        while (*p && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            p++;
        }
    }

    return p;
}

/**
 * Parse a single title object from JSON.
 */
bool ParseTitleObject(const char* objStart, const char* objEnd, TitlePreset* preset) {
    // Create a null-terminated copy of the object
    int objLen = objEnd - objStart;
    char* obj = new char[objLen + 1];
    memcpy(obj, objStart, objLen);
    obj[objLen] = '\0';

    bool success = false;
    char tempStr[MAX_PRESET_NAME];

    // Required: id (game ID / product code)
    const char* val = FindKey(obj, "id");
    if (val && *val == '"') {
        ParseString(val, preset->gameId, MAX_GAME_ID);
        if (preset->gameId[0] != '\0') {
            success = true;
        }
    }

    if (!success) {
        delete[] obj;
        return false;
    }

    // Required: name
    val = FindKey(obj, "name");
    if (val && *val == '"') {
        ParseString(val, preset->name, MAX_PRESET_NAME);
    }

    // Optional: publisher
    val = FindKey(obj, "publisher");
    if (val && *val == '"') {
        ParseString(val, preset->publisher, MAX_PUBLISHER_NAME);
    }

    // Optional: developer
    val = FindKey(obj, "developer");
    if (val && *val == '"') {
        ParseString(val, preset->developer, MAX_DEVELOPER_NAME);
    }

    // Optional: releaseDate
    val = FindKey(obj, "releaseDate");
    if (val && *val == '"') {
        ParseString(val, tempStr, sizeof(tempStr));
        ParseDate(tempStr, &preset->releaseYear, &preset->releaseMonth, &preset->releaseDay);
    }

    // Optional: region
    val = FindKey(obj, "region");
    if (val && *val == '"') {
        ParseString(val, preset->region, MAX_REGION_NAME);
    }

    // Optional: genre
    val = FindKey(obj, "genre");
    if (val && *val == '"') {
        ParseString(val, preset->genre, MAX_GENRE_NAME);
    }

    delete[] obj;
    return true;
}

/**
 * Parse the titles array from JSON.
 */
int ParseTitlesArray(const char* json) {
    int count = 0;

    // Find the "titles" array
    const char* val = FindKey(json, "titles");
    if (!val || *val != '[') {
        return 0;
    }

    const char* p = val + 1; // Skip '['
    p = SkipWhitespace(p);

    while (*p && *p != ']' && count < MAX_PRESETS) {
        if (*p == '{') {
            // Find the end of this object
            const char* objStart = p;
            p = SkipValue(p);
            const char* objEnd = p;

            TitlePreset preset;
            if (ParseTitleObject(objStart, objEnd, &preset)) {
                gPresets[count++] = preset;
            }
        }

        p = SkipWhitespace(p);
        if (*p == ',') {
            p++;
            p = SkipWhitespace(p);
        }
    }

    return count;
}

/**
 * Case-insensitive string comparison.
 */
bool StrEqualsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * Check if string 'haystack' ends with 'needle' (case-insensitive).
 */
bool StrEndsWith(const char* haystack, const char* needle) {
    int hLen = strlen(haystack);
    int nLen = strlen(needle);
    if (nLen > hLen) return false;
    return StrEqualsIgnoreCase(haystack + (hLen - nLen), needle);
}

} // anonymous namespace

// =============================================================================
// Core Functions Implementation
// =============================================================================

bool Load() {
    // Reset state
    gPresetCount = 0;
    gIsLoaded = false;

    // Open the presets file
    FILE* file = fopen(PRESETS_FILE_PATH, "rb");
    if (!file) {
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0 || fileSize > 4 * 1024 * 1024) { // Max 4MB
        fclose(file);
        return false;
    }

    // Read file into memory
    char* json = new char[fileSize + 1];
    if (fread(json, 1, fileSize, file) != (size_t)fileSize) {
        delete[] json;
        fclose(file);
        return false;
    }
    json[fileSize] = '\0';
    fclose(file);

    // Parse the JSON
    gPresetCount = ParseTitlesArray(json);
    delete[] json;

    gIsLoaded = (gPresetCount > 0);
    return gIsLoaded;
}

bool IsLoaded() {
    return gIsLoaded;
}

int GetPresetCount() {
    return gPresetCount;
}

void GetStats(PresetStats& stats) {
    stats.totalPresets = gPresetCount;
    stats.titlesWithReleaseDate = 0;

    std::set<std::string> publishers, developers, genres, regions;

    for (int i = 0; i < gPresetCount; i++) {
        const TitlePreset& p = gPresets[i];

        if (p.publisher[0]) publishers.insert(p.publisher);
        if (p.developer[0]) developers.insert(p.developer);
        if (p.genre[0]) genres.insert(p.genre);
        if (p.region[0]) regions.insert(p.region);
        if (p.releaseYear > 0) stats.titlesWithReleaseDate++;
    }

    stats.uniquePublishers = static_cast<int>(publishers.size());
    stats.uniqueDevelopers = static_cast<int>(developers.size());
    stats.uniqueGenres = static_cast<int>(genres.size());
    stats.uniqueRegions = static_cast<int>(regions.size());
}

const TitlePreset* GetPresetByGameId(const char* gameId) {
    if (!gameId || gameId[0] == '\0') {
        return nullptr;
    }

    int searchLen = strlen(gameId);

    for (int i = 0; i < gPresetCount; i++) {
        const char* presetId = gPresets[i].gameId;

        // Exact match
        if (StrEqualsIgnoreCase(presetId, gameId)) {
            return &gPresets[i];
        }

        // Partial match: product code like "WUP-P-ARDE01" should match "ARDE01"
        // Check if the preset's gameId matches the end of the search string
        if (StrEndsWith(gameId, presetId)) {
            return &gPresets[i];
        }

        // Also check if search string matches end of preset's gameId
        // e.g., searching "ARDE" should match preset "ARDE01"
        int presetLen = strlen(presetId);
        if (searchLen <= presetLen && StrEndsWith(presetId, gameId)) {
            return &gPresets[i];
        }

        // Check after last hyphen in search string
        // e.g., "WUP-P-ARDE01" -> check "ARDE01"
        const char* lastHyphen = strrchr(gameId, '-');
        if (lastHyphen && StrEqualsIgnoreCase(lastHyphen + 1, presetId)) {
            return &gPresets[i];
        }
    }

    return nullptr;
}

const TitlePreset* GetPresetByIndex(int index) {
    if (index < 0 || index >= gPresetCount) {
        return nullptr;
    }
    return &gPresets[index];
}

// =============================================================================
// Query Functions Implementation
// =============================================================================

int GetUniquePublishers(std::vector<const char*>& outPublishers) {
    outPublishers.clear();
    std::set<std::string> seen;

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].publisher[0] && seen.find(gPresets[i].publisher) == seen.end()) {
            seen.insert(gPresets[i].publisher);
            outPublishers.push_back(gPresets[i].publisher);
        }
    }

    // Sort alphabetically
    std::sort(outPublishers.begin(), outPublishers.end(),
              [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    return static_cast<int>(outPublishers.size());
}

int GetUniqueDevelopers(std::vector<const char*>& outDevelopers) {
    outDevelopers.clear();
    std::set<std::string> seen;

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].developer[0] && seen.find(gPresets[i].developer) == seen.end()) {
            seen.insert(gPresets[i].developer);
            outDevelopers.push_back(gPresets[i].developer);
        }
    }

    std::sort(outDevelopers.begin(), outDevelopers.end(),
              [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    return static_cast<int>(outDevelopers.size());
}

int GetUniqueGenres(std::vector<const char*>& outGenres) {
    outGenres.clear();
    std::set<std::string> seen;

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].genre[0] && seen.find(gPresets[i].genre) == seen.end()) {
            seen.insert(gPresets[i].genre);
            outGenres.push_back(gPresets[i].genre);
        }
    }

    std::sort(outGenres.begin(), outGenres.end(),
              [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    return static_cast<int>(outGenres.size());
}

int GetUniqueRegions(std::vector<const char*>& outRegions) {
    outRegions.clear();
    std::set<std::string> seen;

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].region[0] && seen.find(gPresets[i].region) == seen.end()) {
            seen.insert(gPresets[i].region);
            outRegions.push_back(gPresets[i].region);
        }
    }

    std::sort(outRegions.begin(), outRegions.end(),
              [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    return static_cast<int>(outRegions.size());
}

int GetUniqueYears(std::vector<uint16_t>& outYears) {
    outYears.clear();
    std::set<uint16_t> seen;

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].releaseYear > 0 && seen.find(gPresets[i].releaseYear) == seen.end()) {
            seen.insert(gPresets[i].releaseYear);
            outYears.push_back(gPresets[i].releaseYear);
        }
    }

    std::sort(outYears.begin(), outYears.end());

    return static_cast<int>(outYears.size());
}

int GetGameIdsByPublisher(const char* publisher, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].publisher, publisher)) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

int GetGameIdsByDeveloper(const char* developer, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].developer, developer)) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

int GetGameIdsByGenre(const char* genre, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].genre, genre)) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

int GetGameIdsByRegion(const char* region, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].region, region)) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

int GetGameIdsByYear(uint16_t year, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].releaseYear == year) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

int GetGameIdsByYearRange(uint16_t startYear, uint16_t endYear, std::vector<const char*>& outGameIds) {
    outGameIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].releaseYear >= startYear && gPresets[i].releaseYear <= endYear) {
            outGameIds.push_back(gPresets[i].gameId);
        }
    }

    return static_cast<int>(outGameIds.size());
}

// =============================================================================
// Category Generation Helpers Implementation
// =============================================================================

int GetSuggestedCategories(PresetCategoryType type,
                           std::vector<SuggestedCategory>& outSuggestions,
                           int minTitles) {
    outSuggestions.clear();

    switch (type) {
        case PresetCategoryType::Publisher: {
            std::vector<const char*> publishers;
            GetUniquePublishers(publishers);
            for (const char* pub : publishers) {
                std::vector<const char*> gameIds;
                GetGameIdsByPublisher(pub, gameIds);
                if (static_cast<int>(gameIds.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, pub, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(gameIds.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Developer: {
            std::vector<const char*> developers;
            GetUniqueDevelopers(developers);
            for (const char* dev : developers) {
                std::vector<const char*> gameIds;
                GetGameIdsByDeveloper(dev, gameIds);
                if (static_cast<int>(gameIds.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, dev, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(gameIds.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Genre: {
            std::vector<const char*> genres;
            GetUniqueGenres(genres);
            for (const char* genre : genres) {
                std::vector<const char*> gameIds;
                GetGameIdsByGenre(genre, gameIds);
                if (static_cast<int>(gameIds.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, genre, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(gameIds.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Region: {
            std::vector<const char*> regions;
            GetUniqueRegions(regions);
            for (const char* region : regions) {
                std::vector<const char*> gameIds;
                GetGameIdsByRegion(region, gameIds);
                if (static_cast<int>(gameIds.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, region, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(gameIds.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::ReleaseYear: {
            std::vector<uint16_t> years;
            GetUniqueYears(years);
            for (uint16_t year : years) {
                std::vector<const char*> gameIds;
                GetGameIdsByYear(year, gameIds);
                if (static_cast<int>(gameIds.size()) >= minTitles) {
                    SuggestedCategory cat;
                    snprintf(cat.name, MAX_PUBLISHER_NAME, "%d", year);
                    cat.titleCount = static_cast<int>(gameIds.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::ReleasePeriod: {
            // Group by 2-year periods (e.g., 2012-2013, 2014-2015)
            std::vector<uint16_t> years;
            GetUniqueYears(years);
            if (!years.empty()) {
                uint16_t minYear = years.front();
                uint16_t maxYear = years.back();
                // Round down to even year
                minYear = (minYear / 2) * 2;

                for (uint16_t y = minYear; y <= maxYear; y += 2) {
                    std::vector<const char*> gameIds;
                    GetGameIdsByYearRange(y, y + 1, gameIds);
                    if (static_cast<int>(gameIds.size()) >= minTitles) {
                        SuggestedCategory cat;
                        snprintf(cat.name, MAX_PUBLISHER_NAME, "%d-%d", y, y + 1);
                        cat.titleCount = static_cast<int>(gameIds.size());
                        outSuggestions.push_back(cat);
                    }
                }
            }
            break;
        }
    }

    // Sort by title count (descending)
    std::sort(outSuggestions.begin(), outSuggestions.end(),
              [](const SuggestedCategory& a, const SuggestedCategory& b) {
                  return a.titleCount > b.titleCount;
              });

    return static_cast<int>(outSuggestions.size());
}

} // namespace TitlePresets
