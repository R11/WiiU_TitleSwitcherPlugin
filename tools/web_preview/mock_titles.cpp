/**
 * Mock Titles for Web Preview
 *
 * Provides sample title data for testing the menu UI.
 */

#include "titles/titles.h"
#include <cstring>
#include <algorithm>

namespace Titles {

// Sample titles for preview
static TitleInfo sTitles[] = {
    { 0x0005000010101D00, "Super Mario 3D World", "WUP-P-ARDE" },
    { 0x000500001010EC00, "Mario Kart 8", "WUP-P-AMKE" },
    { 0x0005000010145000, "Super Smash Bros. for Wii U", "WUP-P-AXFE" },
    { 0x000500001019E500, "The Legend of Zelda: Breath of the Wild", "WUP-P-ALZE" },
    { 0x0005000010176A00, "Splatoon", "WUP-P-AGME" },
    { 0x0005000010116100, "New Super Mario Bros. U", "WUP-P-ARPE" },
    { 0x000500001014B700, "Donkey Kong Country: Tropical Freeze", "WUP-P-ARKP" },
    { 0x000500001010ED00, "Pikmin 3", "WUP-P-AC3E" },
    { 0x0005000010137F00, "Bayonetta 2", "WUP-P-AQBE" },
    { 0x0005000010144F00, "Captain Toad: Treasure Tracker", "WUP-P-AKBE" },
    { 0x00050000101A5600, "Yoshi's Woolly World", "WUP-P-AYCE" },
    { 0x0005000010185200, "Xenoblade Chronicles X", "WUP-P-AX5E" },
    { 0x0005000010175B00, "Kirby and the Rainbow Curse", "WUP-P-AXXE" },
    { 0x0005000010106200, "Nintendo Land", "WUP-P-ALCE" },
    { 0x0005000010172600, "Hyrule Warriors", "WUP-P-BWPE" },
    { 0x0005000010150000, "Tokyo Mirage Sessions #FE", "WUP-P-ASEE" },
    { 0x00050000101C9300, "Paper Mario: Color Splash", "WUP-P-CNFE" },
    { 0x00050000101D7500, "Star Fox Zero", "WUP-P-AFXE" },
    { 0x00050000101BA400, "Pokken Tournament", "WUP-P-APKE" },
    { 0x0005000010180600, "The Wonderful 101", "WUP-P-ACME" },
    { 0x0005000010104D00, "ZombiU", "WUP-P-AZUE" },
    { 0x0005000010162B00, "Rayman Legends", "WUP-P-ARME" },
    { 0x0005000010148300, "Mario Tennis: Ultra Smash", "WUP-P-AVXE" },
    { 0x000500001018DC00, "Mario Party 10", "WUP-P-ABAE" },
    { 0x00050000101A9300, "Animal Crossing: amiibo Festival", "WUP-P-AALP" },
};

static int sTitleCount = sizeof(sTitles) / sizeof(sTitles[0]);
static bool sLoaded = false;

void Load(bool forceReload) {
    (void)forceReload;
    sLoaded = true;
}

bool IsLoaded() {
    return sLoaded;
}

void Clear() {
    // Keep sample data, just mark as not loaded
    sLoaded = false;
}

int GetCount() {
    return sTitleCount;
}

const TitleInfo* GetTitle(int index) {
    if (index < 0 || index >= sTitleCount) {
        return nullptr;
    }
    return &sTitles[index];
}

const TitleInfo* FindById(uint64_t titleId) {
    for (int i = 0; i < sTitleCount; i++) {
        if (sTitles[i].titleId == titleId) {
            return &sTitles[i];
        }
    }
    return nullptr;
}

int FindIndexById(uint64_t titleId) {
    for (int i = 0; i < sTitleCount; i++) {
        if (sTitles[i].titleId == titleId) {
            return i;
        }
    }
    return -1;
}

const TitleInfo* FindByProductCode(const char* productCode) {
    if (!productCode) return nullptr;

    for (int i = 0; i < sTitleCount; i++) {
        if (strstr(sTitles[i].productCode, productCode) != nullptr) {
            return &sTitles[i];
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
