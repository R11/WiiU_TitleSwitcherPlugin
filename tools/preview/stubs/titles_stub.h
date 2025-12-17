/**
 * Titles Stub for Desktop Preview
 *
 * Provides mock title data for testing the menu rendering.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace Titles {

constexpr int MAX_TITLES = 512;
constexpr int MAX_NAME_LENGTH = 64;
constexpr int MAX_PRODUCT_CODE = 24;

struct TitleInfo {
    uint64_t titleId;
    char name[MAX_NAME_LENGTH];
    char productCode[MAX_PRODUCT_CODE];
};

// Mock title data with product codes for GameTDB matching
inline TitleInfo sMockTitles[] = {
    { 0x0005000010145D00, "Super Mario 3D World", "ARDP01" },
    { 0x000500001010EC00, "Mario Kart 8", "AMKP01" },
    { 0x0005000010176900, "Splatoon", "AGMP01" },
    { 0x0005000010145000, "Super Smash Bros. for Wii U", "AXFP01" },
    { 0x00050000101C9400, "The Legend of Zelda: Breath of the Wild", "ALZP01" },
    { 0x0005000010172600, "Bayonetta 2", "AQUE01" },
    { 0x0005000010145200, "Pikmin 3", "AC3P01" },
    { 0x0005000010116100, "Xenoblade Chronicles X", "AX5P01" },
    { 0x0005000010143500, "Hyrule Warriors", "BWPP01" },
    { 0x0005000010138300, "Donkey Kong Country: Tropical Freeze", "ARKP01" },
    { 0x0005000010175B00, "Captain Toad: Treasure Tracker", "AKBP01" },
    { 0x0005000010184D00, "Yoshi's Woolly World", "AXYP01" },
    { 0x0005000010185300, "Star Fox Zero", "AFXP01" },
    { 0x0005000010132200, "NES Remix Pack", "AFCP01" },
    { 0x000500301001500A, "Mii Verse", "" },
    { 0x000500301001200A, "Nintendo eShop", "" },
    { 0x000500301001000A, "Internet Browser", "" },
    { 0x0005000010199500, "Tokyo Mirage Sessions FE", "ASEP01" },
    { 0x0005000010180600, "Pokken Tournament", "APKP01" },
    { 0x0005000010162B00, "Super Mario Maker", "AMFP01" },
};

inline int sMockTitleCount = sizeof(sMockTitles) / sizeof(sMockTitles[0]);

inline void Load(bool forceReload = false) {
    (void)forceReload;
}

inline bool IsLoaded() {
    return true;
}

inline void Clear() {}

inline int GetCount() {
    return sMockTitleCount;
}

inline const TitleInfo* GetTitle(int index) {
    if (index < 0 || index >= sMockTitleCount) return nullptr;
    return &sMockTitles[index];
}

inline const TitleInfo* FindById(uint64_t titleId) {
    for (int i = 0; i < sMockTitleCount; i++) {
        if (sMockTitles[i].titleId == titleId) {
            return &sMockTitles[i];
        }
    }
    return nullptr;
}

inline int FindIndexById(uint64_t titleId) {
    for (int i = 0; i < sMockTitleCount; i++) {
        if (sMockTitles[i].titleId == titleId) {
            return i;
        }
    }
    return -1;
}

inline void GetNameForId(uint64_t titleId, char* outName, int maxLen) {
    const TitleInfo* title = FindById(titleId);
    if (title) {
        strncpy(outName, title->name, maxLen - 1);
        outName[maxLen - 1] = '\0';
    } else {
        snprintf(outName, maxLen, "%016llX", (unsigned long long)titleId);
    }
}

} // namespace Titles
