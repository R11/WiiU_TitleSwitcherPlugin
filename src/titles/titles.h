/**
 * Title Management System
 *
 * This module handles enumeration, caching, and metadata retrieval for
 * installed Wii U titles (games, apps, etc.).
 *
 * KEY CONCEPTS:
 * -------------
 *
 * Title ID:
 *   A 64-bit unique identifier for every Wii U title. The format is:
 *     0x000500001XXXXXXX - Games and applications
 *     0x00050010XXXXXXXX - System applications
 *
 *   The lower 32 bits are typically the "game ID" while upper bits indicate
 *   the title type and region.
 *
 * MCP (Master Control Program):
 *   The Wii U system service that manages installed titles. We use it to
 *   enumerate what's installed via MCP_TitleListByAppType().
 *
 * ACP (Application Configuration Program):
 *   The system service for title metadata. We use ACPGetTitleMetaXml() to
 *   get human-readable title names from the installed title's meta.xml.
 *
 * USAGE:
 * ------
 *   // At startup (can take a few seconds):
 *   Titles::Load();
 *
 *   // Get title count:
 *   int count = Titles::GetCount();
 *
 *   // Get title info:
 *   const Titles::TitleInfo* title = Titles::GetTitle(index);
 *   if (title) {
 *       printf("Name: %s\n", title->name);
 *       printf("ID: %016llX\n", title->titleId);
 *   }
 *
 * CACHING:
 * --------
 * Title enumeration is slow (several seconds for a large library) because
 * we need to read metadata from each title. The results are cached in memory
 * after the first load, so subsequent calls to Load() return immediately.
 *
 * To force a refresh (e.g., if user installed new games), call:
 *   Titles::Load(true);  // Force reload
 *
 * REFERENCE:
 * ----------
 * - MCP API: https://wut.devkitpro.org/group__coreinit__mcp.html
 * - ACP API: https://wut.devkitpro.org/nn__acp.html
 */

#pragma once

#include <cstdint>

namespace Titles {

// =============================================================================
// Constants
// =============================================================================

// Maximum number of titles we can store
// 512 should be more than enough for any Wii U library
constexpr int MAX_TITLES = 512;

// Maximum length of a title name (including null terminator)
constexpr int MAX_NAME_LENGTH = 64;

// =============================================================================
// Data Structures
// =============================================================================

/**
 * Information about a single installed title.
 *
 * This structure is populated by Load() and represents one game or app
 * that can be launched by the user.
 */
struct TitleInfo {
    // 64-bit unique identifier for this title
    // Used with SYSLaunchTitle() to launch the game
    uint64_t titleId;

    // Human-readable name (from meta.xml)
    // Falls back to hex title ID if name unavailable
    char name[MAX_NAME_LENGTH];
};

// =============================================================================
// Loading Functions
// =============================================================================

/**
 * Load (or reload) the list of installed titles.
 *
 * This function queries the system for all installed games and retrieves
 * their metadata (names). The results are cached for fast subsequent access.
 *
 * The first call may take several seconds depending on library size.
 * Subsequent calls return immediately unless forceReload is true.
 *
 * @param forceReload If true, clear cache and reload from system even if
 *                    already loaded. Use this after game installs/uninstalls.
 *
 * Titles are automatically sorted alphabetically by name after loading.
 *
 * NOTE: This function filters out the currently running title to prevent
 *       the user from "launching" the game they're already in.
 */
void Load(bool forceReload = false);

/**
 * Check if titles have been loaded.
 *
 * @return true if Load() has been called and completed successfully
 */
bool IsLoaded();

/**
 * Clear the cached title list.
 *
 * After calling this, GetCount() will return 0 and IsLoaded() will return false.
 * The next call to Load() will re-enumerate titles from the system.
 */
void Clear();

// =============================================================================
// Access Functions
// =============================================================================

/**
 * Get the number of loaded titles.
 *
 * @return Number of titles, or 0 if Load() hasn't been called
 */
int GetCount();

/**
 * Get a title by index.
 *
 * @param index Zero-based index (0 to GetCount()-1)
 * @return Pointer to TitleInfo, or nullptr if index is out of range
 *
 * The returned pointer is valid until Clear() or Load(true) is called.
 *
 * Example:
 *   for (int i = 0; i < Titles::GetCount(); i++) {
 *       const auto* title = Titles::GetTitle(i);
 *       printf("%d: %s\n", i, title->name);
 *   }
 */
const TitleInfo* GetTitle(int index);

/**
 * Find a title by its ID.
 *
 * @param titleId The 64-bit title ID to search for
 * @return Pointer to TitleInfo, or nullptr if not found
 *
 * This performs a linear search through the title list.
 *
 * Example:
 *   const auto* mkart = Titles::FindById(0x000500001010EC00);  // Mario Kart 8 USA
 *   if (mkart) {
 *       printf("Found: %s\n", mkart->name);
 *   }
 */
const TitleInfo* FindById(uint64_t titleId);

/**
 * Find the index of a title by its ID.
 *
 * @param titleId The 64-bit title ID to search for
 * @return Index of the title (0 to GetCount()-1), or -1 if not found
 *
 * Example:
 *   int idx = Titles::FindIndexById(savedTitleId);
 *   if (idx >= 0) {
 *       // Restore selection to this index
 *   }
 */
int FindIndexById(uint64_t titleId);

// =============================================================================
// Metadata Functions
// =============================================================================

/**
 * Get the display name for a title ID.
 *
 * This function looks up the title's metadata and returns a human-readable name.
 * If the title isn't in our cache or metadata isn't available, falls back to
 * displaying the hex title ID.
 *
 * @param titleId  The title to look up
 * @param outName  Buffer to receive the name
 * @param maxLen   Size of the output buffer
 *
 * Example:
 *   char name[64];
 *   Titles::GetNameForId(0x000500001010EC00, name, sizeof(name));
 *   printf("Name: %s\n", name);  // "Mario Kart 8" or "000500001010EC00"
 */
void GetNameForId(uint64_t titleId, char* outName, int maxLen);

} // namespace Titles
