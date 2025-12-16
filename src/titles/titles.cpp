/**
 * Title Management Implementation
 *
 * See titles.h for usage documentation.
 */

#include "titles.h"

// Wii U SDK headers
#include <coreinit/mcp.h>         // MCP_Open, MCP_TitleListByAppType, etc.
#include <coreinit/title.h>       // OSGetTitleID
#include <nn/acp/title.h>         // ACPGetTitleMetaXml, ACPMetaXml

// Standard library
#include <cstring>                // strncpy, memset
#include <cstdio>                 // snprintf
#include <cctype>                 // strcasecmp
#include <malloc.h>               // memalign, free

namespace Titles {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// The cached list of titles
TitleInfo sTitles[MAX_TITLES];

// Number of titles in the cache
int sTitleCount = 0;

// Has Load() been called successfully?
bool sIsLoaded = false;

// -----------------------------------------------------------------------------
// Helper: Retrieve title name from system metadata
// -----------------------------------------------------------------------------
// This function reads the title's meta.xml file to get its human-readable name.
// The meta.xml contains localized names in multiple languages; we prefer English.
//
// The ACPMetaXml structure is quite large (~20KB) and must be aligned for
// proper access, so we allocate it dynamically with memalign().
//
// Reference: https://wut.devkitpro.org/nn__acp.html
// -----------------------------------------------------------------------------
void getTitleNameFromSystem(uint64_t titleId, char* outName, int maxLen)
{
    // Allocate aligned memory for the metadata structure
    // The structure must be 0x40 (64) byte aligned for the ACP functions
    ACPMetaXml* metaXml = static_cast<ACPMetaXml*>(memalign(0x40, sizeof(ACPMetaXml)));

    if (!metaXml) {
        // Allocation failed - fall back to hex ID
        snprintf(outName, maxLen, "%016llX", static_cast<unsigned long long>(titleId));
        return;
    }

    // Zero the structure before use
    memset(metaXml, 0, sizeof(ACPMetaXml));

    // Try to get the metadata from the system
    // This reads the title's /meta/meta.xml file
    ACPResult result = ACPGetTitleMetaXml(titleId, metaXml);

    if (result == ACP_RESULT_SUCCESS) {
        // Try to find a name, preferring English
        // The meta.xml contains multiple name fields for different languages:
        //   shortname_en - Short English name (preferred)
        //   longname_en  - Long English name (fallback)
        //   shortname_ja - Short Japanese name (last resort)
        const char* name = nullptr;

        if (metaXml->shortname_en[0] != '\0') {
            name = metaXml->shortname_en;
        } else if (metaXml->longname_en[0] != '\0') {
            name = metaXml->longname_en;
        } else if (metaXml->shortname_ja[0] != '\0') {
            name = metaXml->shortname_ja;
        }

        if (name) {
            // Copy the name, limiting to our max length
            // Use %.63s to ensure null termination
            snprintf(outName, maxLen, "%.63s", name);
        } else {
            // No name found in metadata - use hex ID
            snprintf(outName, maxLen, "%016llX", static_cast<unsigned long long>(titleId));
        }
    } else {
        // Metadata retrieval failed - use hex ID
        // This can happen for system titles or corrupted installations
        snprintf(outName, maxLen, "%016llX", static_cast<unsigned long long>(titleId));
    }

    // Free the metadata structure
    free(metaXml);
}

// -----------------------------------------------------------------------------
// Helper: Sort titles alphabetically by name
// -----------------------------------------------------------------------------
// Simple bubble sort is fine here since we only run this once at load time
// and title counts are small (<500 typically).
// -----------------------------------------------------------------------------
void sortTitlesAlphabetically()
{
    for (int i = 0; i < sTitleCount - 1; i++) {
        for (int j = 0; j < sTitleCount - i - 1; j++) {
            // Case-insensitive comparison
            if (strcasecmp(sTitles[j].name, sTitles[j + 1].name) > 0) {
                // Swap titles
                TitleInfo temp = sTitles[j];
                sTitles[j] = sTitles[j + 1];
                sTitles[j + 1] = temp;
            }
        }
    }
}

} // anonymous namespace

// =============================================================================
// Public Implementation
// =============================================================================

void Load(bool forceReload)
{
    // Return cached data if available and not forcing reload
    if (sIsLoaded && !forceReload) {
        return;
    }

    // Reset state
    sTitleCount = 0;
    sIsLoaded = false;

    // -------------------------------------------------------------------------
    // Get the current title ID to exclude from the list
    // -------------------------------------------------------------------------
    // We don't want to show the currently running game in the launcher,
    // as "launching" it would just restart it (and might cause issues).
    uint64_t currentTitleId = OSGetTitleID();

    // -------------------------------------------------------------------------
    // Open connection to MCP (Master Control Program)
    // -------------------------------------------------------------------------
    // MCP is the system service that manages installed titles.
    // We need a handle to query the title list.
    //
    // Reference: https://wut.devkitpro.org/group__coreinit__mcp.html
    int32_t mcpHandle = MCP_Open();
    if (mcpHandle < 0) {
        // Failed to open MCP - can't enumerate titles
        // This shouldn't happen on a normal system
        sIsLoaded = true;  // Mark as loaded (with 0 titles) to prevent repeated attempts
        return;
    }

    // -------------------------------------------------------------------------
    // Allocate temporary buffer for title list
    // -------------------------------------------------------------------------
    // MCP_TitleListByAppType() writes MCPTitleListType structures to a buffer.
    // We need to allocate enough space for all potential titles.
    MCPTitleListType* titleList = static_cast<MCPTitleListType*>(
        malloc(sizeof(MCPTitleListType) * MAX_TITLES)
    );

    if (!titleList) {
        // Allocation failed
        MCP_Close(mcpHandle);
        sIsLoaded = true;
        return;
    }

    // -------------------------------------------------------------------------
    // Query installed games
    // -------------------------------------------------------------------------
    // MCP_TitleListByAppType() returns titles of a specific type.
    // MCP_APP_TYPE_GAME includes retail games and eShop purchases.
    //
    // The function fills titleList with MCPTitleListType structures and
    // returns the count via the count pointer.
    uint32_t count = 0;
    MCPError err = MCP_TitleListByAppType(
        mcpHandle,
        MCP_APP_TYPE_GAME,                        // Title type to query
        &count,                                    // Output: number of titles found
        titleList,                                 // Output: array of title info
        sizeof(MCPTitleListType) * MAX_TITLES      // Buffer size
    );

    if (err >= 0 && count > 0) {
        // Process each title
        for (uint32_t i = 0; i < count && sTitleCount < MAX_TITLES; i++) {
            uint64_t titleId = titleList[i].titleId;

            // Skip the currently running title
            if (titleId == currentTitleId) {
                continue;
            }

            // Store the title ID
            sTitles[sTitleCount].titleId = titleId;

            // Get the human-readable name
            getTitleNameFromSystem(titleId, sTitles[sTitleCount].name, MAX_NAME_LENGTH);

            sTitleCount++;
        }
    }

    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------
    free(titleList);
    MCP_Close(mcpHandle);

    // -------------------------------------------------------------------------
    // Sort titles alphabetically
    // -------------------------------------------------------------------------
    // This makes the list easier to navigate for users
    sortTitlesAlphabetically();

    sIsLoaded = true;
}

bool IsLoaded()
{
    return sIsLoaded;
}

void Clear()
{
    sTitleCount = 0;
    sIsLoaded = false;
}

int GetCount()
{
    return sTitleCount;
}

const TitleInfo* GetTitle(int index)
{
    if (index < 0 || index >= sTitleCount) {
        return nullptr;
    }
    return &sTitles[index];
}

const TitleInfo* FindById(uint64_t titleId)
{
    for (int i = 0; i < sTitleCount; i++) {
        if (sTitles[i].titleId == titleId) {
            return &sTitles[i];
        }
    }
    return nullptr;
}

int FindIndexById(uint64_t titleId)
{
    for (int i = 0; i < sTitleCount; i++) {
        if (sTitles[i].titleId == titleId) {
            return i;
        }
    }
    return -1;
}

void GetNameForId(uint64_t titleId, char* outName, int maxLen)
{
    // First check our cache
    const TitleInfo* cached = FindById(titleId);
    if (cached) {
        strncpy(outName, cached->name, maxLen - 1);
        outName[maxLen - 1] = '\0';
        return;
    }

    // Not in cache - query system directly
    getTitleNameFromSystem(titleId, outName, maxLen);
}

} // namespace Titles
