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
 * Parse a JSON number (integer only).
 */
const char* ParseNumber(const char* p, int* outValue) {
    *outValue = 0;
    bool negative = false;

    if (*p == '-') {
        negative = true;
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        *outValue = (*outValue * 10) + (*p - '0');
        p++;
    }

    if (negative) *outValue = -*outValue;
    return p;
}

/**
 * Parse a hex string (16 chars) into a uint64_t title ID.
 */
bool ParseTitleId(const char* hexStr, uint64_t* outId) {
    *outId = 0;

    for (int i = 0; i < 16; i++) {
        char c = hexStr[i];
        uint64_t nibble;

        if (c >= '0' && c <= '9') {
            nibble = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            nibble = 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'f') {
            nibble = 10 + (c - 'a');
        } else {
            return false;
        }

        *outId = (*outId << 4) | nibble;
    }

    return true;
}

/**
 * Parse a date string (YYYY-MM-DD or YYYY) into components.
 */
void ParseDate(const char* dateStr, uint16_t* year, uint8_t* month, uint8_t* day) {
    *year = 0;
    *month = 0;
    *day = 0;

    // Parse year
    if (strlen(dateStr) >= 4) {
        *year = (dateStr[0] - '0') * 1000 +
                (dateStr[1] - '0') * 100 +
                (dateStr[2] - '0') * 10 +
                (dateStr[3] - '0');
    }

    // Parse month if present (YYYY-MM-DD format)
    if (strlen(dateStr) >= 7 && dateStr[4] == '-') {
        *month = (dateStr[5] - '0') * 10 + (dateStr[6] - '0');
    }

    // Parse day if present
    if (strlen(dateStr) >= 10 && dateStr[7] == '-') {
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

    // Required: titleId
    const char* val = FindKey(obj, "titleId");
    if (val && *val == '"') {
        val++;
        char idStr[17] = {0};
        for (int i = 0; i < 16 && val[i] && val[i] != '"'; i++) {
            idStr[i] = val[i];
        }
        if (ParseTitleId(idStr, &preset->titleId)) {
            success = true;
        }
    }

    if (!success) {
        delete[] obj;
        return false;
    }

    // Required: name
    val = FindKey(obj, "name");
    if (val) {
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

const TitlePreset* GetPreset(uint64_t titleId) {
    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].titleId == titleId) {
            return &gPresets[i];
        }
    }
    return nullptr;
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

int GetTitlesByPublisher(const char* publisher, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].publisher, publisher)) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
}

int GetTitlesByDeveloper(const char* developer, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].developer, developer)) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
}

int GetTitlesByGenre(const char* genre, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].genre, genre)) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
}

int GetTitlesByRegion(const char* region, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (StrEqualsIgnoreCase(gPresets[i].region, region)) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
}

int GetTitlesByYear(uint16_t year, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].releaseYear == year) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
}

int GetTitlesByYearRange(uint16_t startYear, uint16_t endYear, std::vector<uint64_t>& outTitleIds) {
    outTitleIds.clear();

    for (int i = 0; i < gPresetCount; i++) {
        if (gPresets[i].releaseYear >= startYear && gPresets[i].releaseYear <= endYear) {
            outTitleIds.push_back(gPresets[i].titleId);
        }
    }

    return static_cast<int>(outTitleIds.size());
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
                std::vector<uint64_t> titles;
                GetTitlesByPublisher(pub, titles);
                if (static_cast<int>(titles.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, pub, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(titles.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Developer: {
            std::vector<const char*> developers;
            GetUniqueDevelopers(developers);
            for (const char* dev : developers) {
                std::vector<uint64_t> titles;
                GetTitlesByDeveloper(dev, titles);
                if (static_cast<int>(titles.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, dev, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(titles.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Genre: {
            std::vector<const char*> genres;
            GetUniqueGenres(genres);
            for (const char* genre : genres) {
                std::vector<uint64_t> titles;
                GetTitlesByGenre(genre, titles);
                if (static_cast<int>(titles.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, genre, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(titles.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::Region: {
            std::vector<const char*> regions;
            GetUniqueRegions(regions);
            for (const char* region : regions) {
                std::vector<uint64_t> titles;
                GetTitlesByRegion(region, titles);
                if (static_cast<int>(titles.size()) >= minTitles) {
                    SuggestedCategory cat;
                    strncpy(cat.name, region, MAX_PUBLISHER_NAME - 1);
                    cat.name[MAX_PUBLISHER_NAME - 1] = '\0';
                    cat.titleCount = static_cast<int>(titles.size());
                    outSuggestions.push_back(cat);
                }
            }
            break;
        }

        case PresetCategoryType::ReleaseYear: {
            std::vector<uint16_t> years;
            GetUniqueYears(years);
            for (uint16_t year : years) {
                std::vector<uint64_t> titles;
                GetTitlesByYear(year, titles);
                if (static_cast<int>(titles.size()) >= minTitles) {
                    SuggestedCategory cat;
                    snprintf(cat.name, MAX_PUBLISHER_NAME, "%d", year);
                    cat.titleCount = static_cast<int>(titles.size());
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
                    std::vector<uint64_t> titles;
                    GetTitlesByYearRange(y, y + 1, titles);
                    if (static_cast<int>(titles.size()) >= minTitles) {
                        SuggestedCategory cat;
                        snprintf(cat.name, MAX_PUBLISHER_NAME, "%d-%d", y, y + 1);
                        cat.titleCount = static_cast<int>(titles.size());
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
