/**
 * Centralized Path Definitions
 *
 * All file paths used by the plugin are defined here for easy configuration.
 * This makes it simple to change paths without hunting through the codebase.
 *
 * PATH FORMAT NOTES:
 * -----------------
 * - WUT devoptab uses "fs:/vol/external01/" prefix to access SD card
 * - This maps to "sd:/" when viewed from the Wii U system menu
 * - Example: "fs:/vol/external01/wiiu/" == "sd:/wiiu/"
 *
 * SD CARD STRUCTURE:
 * -----------------
 * sd:/
 * └── wiiu/
 *     ├── environments/
 *     │   └── aroma/
 *     │       └── plugins/
 *     │           ├── TitleSwitcherPlugin.wps    (plugin binary)
 *     │           └── config/
 *     │               └── TitleSwitcher_presets.json (metadata)
 *     │
 *     ├── plugins/
 *     │   └── config/
 *     │       └── TitleSwitcher.json             (settings - auto-created by WUPS)
 *     │
 *     └── titleswitcher/
 *         └── (user-created content like drawings)
 */

#pragma once

namespace Paths {

// =============================================================================
// SD Card Root (WUT devoptab format)
// =============================================================================

constexpr const char* SD_ROOT = "fs:/vol/external01";

// =============================================================================
// Plugin Installation Directory
// =============================================================================

// Where the .wps plugin file is installed (for user reference)
// Actual path: sd:/wiiu/environments/aroma/plugins/
constexpr const char* PLUGIN_DIR = "fs:/vol/external01/wiiu/environments/aroma/plugins";

// =============================================================================
// Configuration Paths
// =============================================================================

// WUPS Storage API config directory (settings are auto-managed by WUPS)
// Settings file is: sd:/wiiu/plugins/config/TitleSwitcher.json
// Note: The actual filename is determined by WUPS_USE_STORAGE() macro in main.cpp
constexpr const char* WUPS_CONFIG_DIR = "fs:/vol/external01/wiiu/plugins/config";

// Aroma plugin config directory (for presets and other plugin-specific data)
constexpr const char* AROMA_CONFIG_DIR = "fs:/vol/external01/wiiu/environments/aroma/plugins/config";

// =============================================================================
// Plugin Data Files
// =============================================================================

// GameTDB presets file - contains metadata for ~2800+ Wii U titles
// Can be regenerated with: tools/convert_gametdb.py
constexpr const char* PRESETS_FILE = "fs:/vol/external01/wiiu/environments/aroma/plugins/config/TitleSwitcher_presets.json";

// User content directory (for pixel editor saves, etc.)
constexpr const char* USER_DATA_DIR = "fs:/vol/external01/wiiu/titleswitcher";

// =============================================================================
// File Names (without path prefix)
// =============================================================================

// Plugin binary filename
constexpr const char* PLUGIN_FILENAME = "TitleSwitcherPlugin.wps";

// Settings filename (managed by WUPS Storage API)
constexpr const char* SETTINGS_FILENAME = "TitleSwitcher.json";

// Presets filename
constexpr const char* PRESETS_FILENAME = "TitleSwitcher_presets.json";

// =============================================================================
// WUPS Storage Namespace
// =============================================================================

// This must match the WUPS_USE_STORAGE() macro in main.cpp
constexpr const char* STORAGE_NAMESPACE = "TitleSwitcher";

// =============================================================================
// SD Card Paths (for documentation/packaging scripts)
// These are the "sd:/" format paths users see on their SD cards
// =============================================================================

namespace SDCard {
    // Plugin installation
    constexpr const char* PLUGIN_DIR = "sd:/wiiu/environments/aroma/plugins";

    // Plugin config (presets)
    constexpr const char* AROMA_CONFIG_DIR = "sd:/wiiu/environments/aroma/plugins/config";

    // WUPS settings (auto-created)
    constexpr const char* WUPS_CONFIG_DIR = "sd:/wiiu/plugins/config";

    // User data
    constexpr const char* USER_DATA_DIR = "sd:/wiiu/titleswitcher";
}

} // namespace Paths
