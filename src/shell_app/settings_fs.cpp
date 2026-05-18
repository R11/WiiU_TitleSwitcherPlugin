/**
 * App-side settings persistence.
 *
 * Reads and writes the shared JSON file the plugin also touches. The plugin
 * additionally writes to WUPS Storage; the app doesn't care about WUPS.
 */

#include "storage/settings.h"
#include "storage/settings_json.h"
#include "storage/file_storage.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

constexpr const char* SHARED_JSON_PATH =
    "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher_settings.json";

}

namespace Settings {

void Load()
{
    PluginSettings& s = Get();

    uint8_t* data = nullptr;
    size_t size = 0;
    if (!FileStorage::ReadFile(SHARED_JSON_PATH, &data, &size)) {
        return;
    }

    std::string_view input(reinterpret_cast<const char*>(data), size);
    Json::Deserialize(input, s);
    free(data);

    Layout::SetCurrentPreferences(s.layoutPrefs);
}

void Save()
{
    std::string json = Json::Serialize(Get());
    FileStorage::WriteFile(SHARED_JSON_PATH,
                           reinterpret_cast<const uint8_t*>(json.data()),
                           json.size());
}

}
