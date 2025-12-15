/**
 * Settings Manager Implementation
 */

#include "settings.h"
#include <wups/storage.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace Settings {

// Global settings instance
PluginSettings gSettings;

// Storage keys
static const char* KEY_VERSION = "configVersion";
static const char* KEY_LAST_INDEX = "lastIndex";
static const char* KEY_BG_COLOR = "bgColor";
static const char* KEY_TITLE_COLOR = "titleColor";
static const char* KEY_HIGHLIGHTED_COLOR = "highlightedColor";
static const char* KEY_FAVORITE_COLOR = "favoriteColor";
static const char* KEY_HEADER_COLOR = "headerColor";
static const char* KEY_FAVORITES_COUNT = "favoritesCount";
static const char* KEY_FAVORITES_DATA = "favoritesData";

void Init()
{
    // Reset to defaults
    gSettings = PluginSettings();
}

void Load()
{
    // Load version first to check for migrations
    int32_t version = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_VERSION, &version);

    if (version == 0) {
        // No saved settings, use defaults
        return;
    }

    // Load integer settings
    WUPSStorageAPI_GetInt(nullptr, KEY_LAST_INDEX, &gSettings.lastIndex);

    // Load colors (stored as signed int, cast to unsigned)
    int32_t tempColor;
    if (WUPSStorageAPI_GetInt(nullptr, KEY_BG_COLOR, &tempColor) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.bgColor = static_cast<uint32_t>(tempColor);
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_TITLE_COLOR, &tempColor) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.titleColor = static_cast<uint32_t>(tempColor);
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_HIGHLIGHTED_COLOR, &tempColor) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.highlightedTitleColor = static_cast<uint32_t>(tempColor);
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_FAVORITE_COLOR, &tempColor) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.favoriteColor = static_cast<uint32_t>(tempColor);
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_HEADER_COLOR, &tempColor) == WUPS_STORAGE_ERROR_SUCCESS) {
        gSettings.headerColor = static_cast<uint32_t>(tempColor);
    }

    // Load favorites
    int32_t favCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_FAVORITES_COUNT, &favCount);

    if (favCount > 0 && favCount <= MAX_FAVORITES) {
        // Favorites stored as binary data (array of uint64_t)
        uint32_t maxSize = favCount * sizeof(uint64_t);
        uint64_t* favData = new uint64_t[favCount];

        uint32_t readSize = 0;
        if (WUPSStorageAPI_GetBinary(nullptr, KEY_FAVORITES_DATA, favData, maxSize, &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            gSettings.favorites.clear();
            for (int32_t i = 0; i < favCount; i++) {
                gSettings.favorites.push_back(favData[i]);
            }
        }

        delete[] favData;
    }

    gSettings.configVersion = version;
}

void Save()
{
    // Save version
    WUPSStorageAPI_StoreInt(nullptr, KEY_VERSION, CONFIG_VERSION);

    // Save integer settings
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAST_INDEX, gSettings.lastIndex);

    // Save colors (cast to signed for storage)
    WUPSStorageAPI_StoreInt(nullptr, KEY_BG_COLOR, static_cast<int32_t>(gSettings.bgColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_TITLE_COLOR, static_cast<int32_t>(gSettings.titleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HIGHLIGHTED_COLOR, static_cast<int32_t>(gSettings.highlightedTitleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITE_COLOR, static_cast<int32_t>(gSettings.favoriteColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HEADER_COLOR, static_cast<int32_t>(gSettings.headerColor));

    // Save favorites
    int32_t favCount = static_cast<int32_t>(gSettings.favorites.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITES_COUNT, favCount);

    if (favCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_FAVORITES_DATA,
                                   gSettings.favorites.data(),
                                   favCount * sizeof(uint64_t));
    }

    // Persist to disk
    WUPSStorageAPI_SaveStorage(false);
}

bool IsFavorite(uint64_t titleId)
{
    return std::find(gSettings.favorites.begin(), gSettings.favorites.end(), titleId)
           != gSettings.favorites.end();
}

void ToggleFavorite(uint64_t titleId)
{
    if (IsFavorite(titleId)) {
        RemoveFavorite(titleId);
    } else {
        AddFavorite(titleId);
    }
}

void AddFavorite(uint64_t titleId)
{
    if (!IsFavorite(titleId) && gSettings.favorites.size() < MAX_FAVORITES) {
        gSettings.favorites.push_back(titleId);
    }
}

void RemoveFavorite(uint64_t titleId)
{
    auto it = std::find(gSettings.favorites.begin(), gSettings.favorites.end(), titleId);
    if (it != gSettings.favorites.end()) {
        gSettings.favorites.erase(it);
    }
}

} // namespace Settings
