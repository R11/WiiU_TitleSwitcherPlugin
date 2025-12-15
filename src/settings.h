/**
 * Settings Manager for Title Switcher
 * Handles loading/saving persistent settings via WUPS Storage API
 */

#pragma once

#include <cstdint>
#include <vector>

namespace Settings {

// Current config version - increment when format changes
constexpr int32_t CONFIG_VERSION = 1;

// Maximum favorites to store
constexpr int MAX_FAVORITES = 64;

// Default colors (Catppuccin Mocha theme)
constexpr uint32_t DEFAULT_BG_COLOR              = 0x1E1E2EFF;  // Base
constexpr uint32_t DEFAULT_TITLE_COLOR           = 0xCDD6F4FF;  // Text
constexpr uint32_t DEFAULT_HIGHLIGHTED_COLOR     = 0x89B4FAFF;  // Blue
constexpr uint32_t DEFAULT_FAVORITE_COLOR        = 0xF9E2AFFF;  // Yellow
constexpr uint32_t DEFAULT_HEADER_COLOR          = 0xA6E3A1FF;  // Green

struct PluginSettings {
    int32_t configVersion;
    int32_t lastIndex;

    // Colors
    uint32_t bgColor;
    uint32_t titleColor;
    uint32_t highlightedTitleColor;
    uint32_t favoriteColor;
    uint32_t headerColor;

    // Favorites (stored as title IDs)
    std::vector<uint64_t> favorites;

    // Default constructor with default values
    PluginSettings() :
        configVersion(CONFIG_VERSION),
        lastIndex(0),
        bgColor(DEFAULT_BG_COLOR),
        titleColor(DEFAULT_TITLE_COLOR),
        highlightedTitleColor(DEFAULT_HIGHLIGHTED_COLOR),
        favoriteColor(DEFAULT_FAVORITE_COLOR),
        headerColor(DEFAULT_HEADER_COLOR)
    {}
};

// Global settings instance
extern PluginSettings gSettings;

// Initialize settings system (call once at plugin init)
void Init();

// Load settings from storage
void Load();

// Save settings to storage
void Save();

// Check if a title is favorited
bool IsFavorite(uint64_t titleId);

// Toggle favorite status for a title
void ToggleFavorite(uint64_t titleId);

// Add a title to favorites
void AddFavorite(uint64_t titleId);

// Remove a title from favorites
void RemoveFavorite(uint64_t titleId);

} // namespace Settings
