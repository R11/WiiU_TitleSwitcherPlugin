/**
 * Mock Titles for Web Preview
 *
 * Loads title data from the embedded presets JSON file,
 * matching how the real Wii U plugin works.
 */

#include "titles/titles.h"
#include "presets/title_presets.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Titles {

static std::vector<TitleInfo> sTitles;
static bool sLoaded = false;

void Load(bool forceReload) {
    if (sLoaded && !forceReload) return;

    sTitles.clear();

    // Load presets from embedded JSON file
    bool presetsLoaded = TitlePresets::Load();
    printf("[Titles] Presets loaded: %s, count: %d\n",
           presetsLoaded ? "yes" : "no", TitlePresets::GetPresetCount());

    int presetCount = TitlePresets::GetPresetCount();
    sTitles.reserve(presetCount);

    for (int i = 0; i < presetCount && i < MAX_TITLES; i++) {
        const auto* preset = TitlePresets::GetPresetByIndex(i);
        if (!preset || preset->name[0] == '\0') continue;

        TitleInfo info;
        // Generate placeholder title ID from index
        info.titleId = 0x0005000010000000ULL + static_cast<uint64_t>(i);

        strncpy(info.name, preset->name, MAX_NAME_LENGTH - 1);
        info.name[MAX_NAME_LENGTH - 1] = '\0';

        strncpy(info.productCode, preset->gameId, MAX_PRODUCT_CODE - 1);
        info.productCode[MAX_PRODUCT_CODE - 1] = '\0';

        sTitles.push_back(info);
    }

    // Sort by name
    std::sort(sTitles.begin(), sTitles.end(), [](const TitleInfo& a, const TitleInfo& b) {
        return strcasecmp(a.name, b.name) < 0;
    });

    sLoaded = true;
}

bool IsLoaded() {
    return sLoaded;
}

void Clear() {
    sTitles.clear();
    sLoaded = false;
}

int GetCount() {
    return static_cast<int>(sTitles.size());
}

const TitleInfo* GetTitle(int index) {
    if (index < 0 || index >= static_cast<int>(sTitles.size())) {
        return nullptr;
    }
    return &sTitles[index];
}

const TitleInfo* FindById(uint64_t titleId) {
    for (const auto& title : sTitles) {
        if (title.titleId == titleId) {
            return &title;
        }
    }
    return nullptr;
}

int FindIndexById(uint64_t titleId) {
    for (size_t i = 0; i < sTitles.size(); i++) {
        if (sTitles[i].titleId == titleId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const TitleInfo* FindByProductCode(const char* productCode) {
    if (!productCode) return nullptr;

    for (const auto& title : sTitles) {
        if (strstr(title.productCode, productCode) != nullptr) {
            return &title;
        }
    }
    return nullptr;
}

void GetNameForId(uint64_t titleId, char* outName, int maxLen) {
    const TitleInfo* title = FindById(titleId);
    if (title) {
        strncpy(outName, title->name, maxLen - 1);
        outName[maxLen - 1] = '\0';
    } else {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
    }
}

} // namespace Titles
