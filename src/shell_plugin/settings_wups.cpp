/**
 * Plugin-side settings persistence.
 *
 * The plugin uses WUPS Storage so its values show up in the Aroma config
 * menu. It ALSO writes a shared JSON file (TitleSwitcher_settings.json) that
 * the standalone app reads. Loading prefers the JSON file when present and
 * falls back to WUPS Storage for migration from older versions.
 */

#include "storage/settings.h"
#include "storage/settings_json.h"
#include "storage/file_storage.h"

#include <wups/storage.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* SHARED_JSON_PATH =
    "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher_settings.json";

constexpr const char* KEY_VERSION          = "configVersion";
constexpr const char* KEY_LAST_INDEX       = "lastIndex";
constexpr const char* KEY_LAST_CATEGORY    = "lastCategory";
constexpr const char* KEY_SHOW_NUMBERS     = "showNumbers";
constexpr const char* KEY_SHOW_FAVORITES   = "showFavorites";
constexpr const char* KEY_BG_COLOR         = "bgColor";
constexpr const char* KEY_TITLE_COLOR      = "titleColor";
constexpr const char* KEY_HIGHLIGHTED      = "highlightedColor";
constexpr const char* KEY_FAVORITE_COLOR   = "favoriteColor";
constexpr const char* KEY_HEADER_COLOR     = "headerColor";
constexpr const char* KEY_CATEGORY_COLOR   = "categoryColor";
constexpr const char* KEY_FAVORITES_COUNT  = "favoritesCount";
constexpr const char* KEY_FAVORITES_DATA   = "favoritesData";
constexpr const char* KEY_CATEGORIES_COUNT = "categoriesCount";
constexpr const char* KEY_CATEGORIES_DATA  = "categoriesData";
constexpr const char* KEY_TITLE_CAT_COUNT  = "titleCatCount";
constexpr const char* KEY_TITLE_CAT_DATA   = "titleCatData";
constexpr const char* KEY_NEXT_CAT_ID      = "nextCategoryId";
constexpr const char* KEY_LAYOUT_FONT_SCALE = "layoutFontScale";
constexpr const char* KEY_LAYOUT_LIST_WIDTH = "layoutListWidth";
constexpr const char* KEY_LAYOUT_ICON_SIZE  = "layoutIconSize";

inline void LoadBool(const char* key, bool& out) {
    int32_t temp;
    if (WUPSStorageAPI_GetInt(nullptr, key, &temp) == WUPS_STORAGE_ERROR_SUCCESS) {
        out = (temp != 0);
    }
}

inline void LoadColor(const char* key, uint32_t& out) {
    int32_t temp;
    if (WUPSStorageAPI_GetInt(nullptr, key, &temp) == WUPS_STORAGE_ERROR_SUCCESS) {
        out = static_cast<uint32_t>(temp);
    }
}

bool LoadFromJsonFile(Settings::PluginSettings& out)
{
    uint8_t* data = nullptr;
    size_t size = 0;
    if (!FileStorage::ReadFile(SHARED_JSON_PATH, &data, &size)) {
        return false;
    }
    std::string_view input(reinterpret_cast<const char*>(data), size);
    bool ok = Settings::Json::Deserialize(input, out);
    free(data);
    return ok;
}

void LoadFromWupsStorage(Settings::PluginSettings& s)
{
    int32_t version = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_VERSION, &version);
    if (version == 0) return;

    WUPSStorageAPI_GetInt(nullptr, KEY_LAST_INDEX, &s.lastIndex);
    WUPSStorageAPI_GetInt(nullptr, KEY_LAST_CATEGORY, &s.lastCategoryIndex);

    LoadBool(KEY_SHOW_NUMBERS, s.showNumbers);
    LoadBool(KEY_SHOW_FAVORITES, s.showFavorites);

    LoadColor(KEY_BG_COLOR, s.bgColor);
    LoadColor(KEY_TITLE_COLOR, s.titleColor);
    LoadColor(KEY_HIGHLIGHTED, s.highlightedTitleColor);
    LoadColor(KEY_FAVORITE_COLOR, s.favoriteColor);
    LoadColor(KEY_HEADER_COLOR, s.headerColor);
    LoadColor(KEY_CATEGORY_COLOR, s.categoryColor);

    int32_t layoutTemp;
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_FONT_SCALE, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        s.layoutPrefs.fontScale = layoutTemp;
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_LIST_WIDTH, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        s.layoutPrefs.listWidthPercent = layoutTemp;
    }
    if (WUPSStorageAPI_GetInt(nullptr, KEY_LAYOUT_ICON_SIZE, &layoutTemp) == WUPS_STORAGE_ERROR_SUCCESS) {
        s.layoutPrefs.iconSizePercent = layoutTemp;
    }

    int32_t nextId;
    if (WUPSStorageAPI_GetInt(nullptr, KEY_NEXT_CAT_ID, &nextId) == WUPS_STORAGE_ERROR_SUCCESS) {
        s.nextCategoryId = static_cast<uint16_t>(nextId);
    }

    int32_t favCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_FAVORITES_COUNT, &favCount);
    if (favCount > 0 && favCount <= Settings::MAX_FAVORITES) {
        uint64_t* favData = new uint64_t[favCount];
        uint32_t readSize = 0;
        if (WUPSStorageAPI_GetBinary(nullptr, KEY_FAVORITES_DATA, favData,
                                      favCount * sizeof(uint64_t), &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            s.favorites.clear();
            for (int32_t i = 0; i < favCount; i++) s.favorites.push_back(favData[i]);
        }
        delete[] favData;
    }

    int32_t catCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_CATEGORIES_COUNT, &catCount);
    if (catCount > 0 && catCount <= Settings::MAX_CATEGORIES) {
        Settings::Category* catData = new Settings::Category[catCount];
        uint32_t readSize = 0;
        if (WUPSStorageAPI_GetBinary(nullptr, KEY_CATEGORIES_DATA, catData,
                                      catCount * sizeof(Settings::Category), &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            s.categories.clear();
            for (int32_t i = 0; i < catCount; i++) s.categories.push_back(catData[i]);
        }
        delete[] catData;
    }

    int32_t tcCount = 0;
    WUPSStorageAPI_GetInt(nullptr, KEY_TITLE_CAT_COUNT, &tcCount);
    if (tcCount > 0 && tcCount <= Settings::MAX_TITLE_CATEGORIES) {
        Settings::TitleCategoryAssignment* tcData = new Settings::TitleCategoryAssignment[tcCount];
        uint32_t readSize = 0;
        if (WUPSStorageAPI_GetBinary(nullptr, KEY_TITLE_CAT_DATA, tcData,
                                      tcCount * sizeof(Settings::TitleCategoryAssignment), &readSize) == WUPS_STORAGE_ERROR_SUCCESS) {
            s.titleCategories.clear();
            for (int32_t i = 0; i < tcCount; i++) s.titleCategories.push_back(tcData[i]);
        }
        delete[] tcData;
    }

    s.configVersion = version;
}

void SaveToWupsStorage(const Settings::PluginSettings& s)
{
    WUPSStorageAPI_StoreInt(nullptr, KEY_VERSION, Settings::CONFIG_VERSION);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAST_INDEX, s.lastIndex);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAST_CATEGORY, s.lastCategoryIndex);
    WUPSStorageAPI_StoreInt(nullptr, KEY_NEXT_CAT_ID, s.nextCategoryId);
    WUPSStorageAPI_StoreInt(nullptr, KEY_SHOW_NUMBERS, s.showNumbers ? 1 : 0);
    WUPSStorageAPI_StoreInt(nullptr, KEY_SHOW_FAVORITES, s.showFavorites ? 1 : 0);
    WUPSStorageAPI_StoreInt(nullptr, KEY_BG_COLOR, static_cast<int32_t>(s.bgColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_TITLE_COLOR, static_cast<int32_t>(s.titleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HIGHLIGHTED, static_cast<int32_t>(s.highlightedTitleColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITE_COLOR, static_cast<int32_t>(s.favoriteColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_HEADER_COLOR, static_cast<int32_t>(s.headerColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_CATEGORY_COLOR, static_cast<int32_t>(s.categoryColor));
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_FONT_SCALE, s.layoutPrefs.fontScale);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_LIST_WIDTH, s.layoutPrefs.listWidthPercent);
    WUPSStorageAPI_StoreInt(nullptr, KEY_LAYOUT_ICON_SIZE, s.layoutPrefs.iconSizePercent);

    int32_t favCount = static_cast<int32_t>(s.favorites.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_FAVORITES_COUNT, favCount);
    if (favCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_FAVORITES_DATA,
                                   s.favorites.data(), favCount * sizeof(uint64_t));
    }

    int32_t catCount = static_cast<int32_t>(s.categories.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_CATEGORIES_COUNT, catCount);
    if (catCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_CATEGORIES_DATA,
                                   s.categories.data(), catCount * sizeof(Settings::Category));
    }

    int32_t tcCount = static_cast<int32_t>(s.titleCategories.size());
    WUPSStorageAPI_StoreInt(nullptr, KEY_TITLE_CAT_COUNT, tcCount);
    if (tcCount > 0) {
        WUPSStorageAPI_StoreBinary(nullptr, KEY_TITLE_CAT_DATA,
                                   s.titleCategories.data(),
                                   tcCount * sizeof(Settings::TitleCategoryAssignment));
    }

    WUPSStorageAPI_SaveStorage(false);
}

}

namespace Settings {

void Load()
{
    PluginSettings& s = Get();
    if (LoadFromJsonFile(s)) {
        Layout::SetCurrentPreferences(s.layoutPrefs);
        return;
    }
    LoadFromWupsStorage(s);
    Layout::SetCurrentPreferences(s.layoutPrefs);
}

void Save()
{
    const PluginSettings& s = Get();
    SaveToWupsStorage(s);

    std::string json = Json::Serialize(s);
    FileStorage::WriteFile(SHARED_JSON_PATH,
                           reinterpret_cast<const uint8_t*>(json.data()),
                           json.size());
}

}
