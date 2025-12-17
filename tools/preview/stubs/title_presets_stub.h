/**
 * Title Presets Stub for Desktop Preview
 *
 * Provides mock preset metadata for testing the menu rendering.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace TitlePresets {

constexpr int MAX_GAME_ID = 16;
constexpr int MAX_PRESET_NAME = 64;
constexpr int MAX_PUBLISHER_NAME = 48;
constexpr int MAX_DEVELOPER_NAME = 48;
constexpr int MAX_REGION_NAME = 16;
constexpr int MAX_GENRE_NAME = 32;

struct TitlePreset {
    char gameId[MAX_GAME_ID];
    char name[MAX_PRESET_NAME];
    char publisher[MAX_PUBLISHER_NAME];
    char developer[MAX_DEVELOPER_NAME];
    uint16_t releaseYear;
    uint8_t releaseMonth;
    uint8_t releaseDay;
    char region[MAX_REGION_NAME];
    char genre[MAX_GENRE_NAME];
};

// Mock preset data
inline TitlePreset sMockPresets[] = {
    { "ARDP01", "Super Mario 3D World", "Nintendo", "Nintendo EAD Tokyo", 2013, 11, 22, "EUR", "Platformer" },
    { "AMKP01", "Mario Kart 8", "Nintendo", "Nintendo EAD", 2014, 5, 30, "EUR", "Racing" },
    { "AGMP01", "Splatoon", "Nintendo", "Nintendo EAD", 2015, 5, 29, "EUR", "Action" },
    { "AXFP01", "Super Smash Bros. for Wii U", "Nintendo", "Bandai Namco/Sora Ltd.", 2014, 11, 28, "EUR", "Fighting" },
    { "ALZP01", "The Legend of Zelda: BotW", "Nintendo", "Nintendo EPD", 2017, 3, 3, "EUR", "Action-Adventure" },
    { "AQUE01", "Bayonetta 2", "Nintendo", "PlatinumGames", 2014, 10, 24, "USA", "Action" },
    { "AC3P01", "Pikmin 3", "Nintendo", "Nintendo EAD", 2013, 7, 26, "EUR", "Strategy" },
    { "AX5P01", "Xenoblade Chronicles X", "Nintendo", "Monolith Soft", 2015, 12, 4, "EUR", "RPG" },
    { "BWPP01", "Hyrule Warriors", "Nintendo", "Omega Force/Team Ninja", 2014, 9, 19, "EUR", "Action" },
    { "ARKP01", "DK Country: Tropical Freeze", "Nintendo", "Retro Studios", 2014, 2, 21, "EUR", "Platformer" },
};

inline int sMockPresetCount = sizeof(sMockPresets) / sizeof(sMockPresets[0]);

inline void Init() {}
inline void Shutdown() {}
inline bool IsLoaded() { return true; }
inline int GetPresetCount() { return sMockPresetCount; }

// Find preset by game ID (flexible matching like the real implementation)
inline const TitlePreset* GetPresetByGameId(const char* gameId) {
    if (!gameId || gameId[0] == '\0') return nullptr;

    // Extract the game ID portion if it's a full product code (e.g., "WUP-P-ARDP01" -> "ARDP01")
    const char* searchId = gameId;
    const char* lastDash = strrchr(gameId, '-');
    if (lastDash) {
        searchId = lastDash + 1;
    }

    for (int i = 0; i < sMockPresetCount; i++) {
        // Try exact match first
        if (strcmp(sMockPresets[i].gameId, searchId) == 0) {
            return &sMockPresets[i];
        }
        // Try partial match (first 4-6 chars)
        if (strncmp(sMockPresets[i].gameId, searchId, 4) == 0) {
            return &sMockPresets[i];
        }
    }
    return nullptr;
}

} // namespace TitlePresets
