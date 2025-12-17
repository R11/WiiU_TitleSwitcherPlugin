/**
 * Title Switcher Plugin for Wii U (Aroma)
 *
 * A custom game launcher menu accessible via button combo (L + R + Minus).
 * Provides category-based organization, favorites, and quick game switching.
 *
 * FEATURES:
 * - Browse installed games with alphabetical sorting
 * - Favorites system (Y to toggle)
 * - Custom categories (create in settings, assign to titles)
 * - ZL/ZR to cycle categories
 * - Split-screen layout with title details
 * - Persistent settings
 *
 * ARCHITECTURE:
 * See docs/ARCHITECTURE.md for detailed system documentation.
 *
 * BUTTON COMBO:
 * L + R + Minus: Open game launcher menu
 * (Configurable in src/input/buttons.h)
 *
 * Author: R11
 * License: GPLv3
 */

// =============================================================================
// WUPS (Wii U Plugin System) Headers
// =============================================================================
// WUPS provides the plugin framework, including:
// - Plugin metadata macros
// - Function hooking system
// - Application lifecycle hooks
// - Storage API for persistent data
//
// Reference: https://github.com/wiiu-env/WiiUPluginSystem
// =============================================================================

#include <wups.h>
#include <wups/config_api.h>

// =============================================================================
// Wii U System Headers
// =============================================================================

#include <vpad/input.h>               // VPADRead, VPADStatus
#include <notifications/notifications.h>  // NotificationModule_*

// =============================================================================
// Plugin Modules
// =============================================================================

#include "menu/menu.h"                // Main menu system
#include "input/buttons.h"            // Button mapping
#include "titles/titles.h"            // Title management
#include "storage/settings.h"         // Persistent settings
#include "presets/title_presets.h"    // GameTDB preset metadata

// =============================================================================
// Plugin Metadata
// =============================================================================
// These macros define the plugin's identity as shown in the Aroma plugin menu.
// =============================================================================

WUPS_PLUGIN_NAME("Title Switcher");
WUPS_PLUGIN_DESCRIPTION("Game launcher menu via L+R+Minus");
WUPS_PLUGIN_VERSION("2.0.0");
WUPS_PLUGIN_AUTHOR("R11");
WUPS_PLUGIN_LICENSE("GPLv3");

// Enable WUT devoptab for file access (not currently used but good to have)
WUPS_USE_WUT_DEVOPTAB();

// Declare storage namespace for WUPS Storage API
// This determines the filename: sd:/wiiu/plugins/config/TitleSwitcher.json
WUPS_USE_STORAGE("TitleSwitcher");

// =============================================================================
// Debug Helpers
// =============================================================================

/**
 * Show a notification message to the user.
 * Useful for debugging and status messages.
 */
static void notify(const char* message)
{
    NotificationModule_AddInfoNotification(message);
}

// =============================================================================
// Plugin Lifecycle
// =============================================================================

/**
 * Plugin initialization - called once when plugin is loaded.
 *
 * This runs before any application starts. We use it to:
 * - Initialize the notification system
 * - Load persistent settings
 * - Preload the title list (for faster menu opening)
 * - Initialize the menu system
 */
INITIALIZE_PLUGIN()
{
    // Initialize notification module for debug messages
    NotificationModule_InitLibrary();

    // Initialize and load settings from SD card
    Settings::Init();
    Settings::Load();

    // Initialize menu system
    Menu::Init();

    // Preload title list in background
    // This takes a few seconds but happens at boot, not when opening the menu
    Titles::Load();

    // Load preset metadata from SD card (GameTDB data)
    // This provides publisher/developer/genre info for installed titles
    TitlePresets::Load();

    // Show startup notification
    notify("Title Switcher ready");
}

/**
 * Plugin deinitialization - called when plugin is unloaded.
 *
 * Clean up any resources we allocated.
 */
DEINITIALIZE_PLUGIN()
{
    Menu::Shutdown();
    NotificationModule_DeInitLibrary();
}

// =============================================================================
// Application Lifecycle Hooks
// =============================================================================
// These hooks track when games start/stop and when they gain/lose focus.
// The menu system uses this information for safety checks.
// =============================================================================

/**
 * Called when a new application (game) starts.
 *
 * We notify the menu system so it can:
 * - Start the grace period timer (prevent opening during loading)
 * - Reset state for the new application
 */
ON_APPLICATION_START()
{
    Menu::OnApplicationStart();
}

/**
 * Called when an application ends.
 *
 * We notify the menu system so it can:
 * - Close the menu if it's open (prevents issues during transitions)
 * - Reset tracking state
 */
ON_APPLICATION_ENDS()
{
    Menu::OnApplicationEnd();
}

/**
 * Called when the application gains foreground focus.
 *
 * This happens after the HOME menu closes or after returning from
 * a system overlay.
 */
ON_ACQUIRED_FOREGROUND()
{
    Menu::OnForegroundAcquired();
}

/**
 * Called when the application loses foreground focus.
 *
 * This happens when the user presses HOME or when a system overlay appears.
 * We close the menu if open to prevent graphical issues.
 */
ON_RELEASE_FOREGROUND()
{
    Menu::OnForegroundReleased();
}

// =============================================================================
// VPADRead Hook - Button Combo Detection
// =============================================================================
// We hook the VPADRead function to intercept GamePad input.
// This allows us to detect the menu button combo before the game sees it.
//
// We have two hooks:
// - VPADRead_Game: For when a game is running
// - VPADRead_Menu: For when on the Wii U Menu
// =============================================================================

namespace {

// Track combo state to detect press vs hold
bool sComboWasHeld = false;

/**
 * Process input and check for menu combo.
 *
 * @param buffer    Input data from VPADRead
 * @param bufferSize Number of samples in buffer
 */
void handleInput(VPADStatus* buffer, uint32_t bufferSize)
{
    // Get current button state
    uint32_t held = buffer[0].hold;

    // Check if menu combo is being held
    bool comboHeld = Buttons::IsComboPressed(held, Buttons::Actions::MENU_OPEN_COMBO);

    // Trigger on press (transition from not-held to held)
    if (comboHeld && !sComboWasHeld && Menu::IsSafeToOpen()) {
        // Open the menu (blocks until menu closes)
        Menu::Open();

        // Clear input buffer so game doesn't see the combo
        // This prevents the game from also responding to the buttons
        for (uint32_t i = 0; i < bufferSize; i++) {
            buffer[i].trigger = 0;
            buffer[i].hold = 0;
            buffer[i].release = 0;
        }
    }

    sComboWasHeld = comboHeld;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// Hook for game processes
// -----------------------------------------------------------------------------
// DECL_FUNCTION declares a function that will replace a system function.
// The 'real_' prefix gives us access to the original function.
// -----------------------------------------------------------------------------

DECL_FUNCTION(int32_t, VPADRead_Game, VPADChan chan, VPADStatus* buffer,
              uint32_t bufferSize, VPADReadError* error)
{
    // Call original function to get real input
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Game(chan, buffer, bufferSize, &realError);

    // Process input if we got valid data
    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleInput(buffer, bufferSize);
    }

    // Pass through the error if caller wants it
    if (error) {
        *error = realError;
    }

    return result;
}

// -----------------------------------------------------------------------------
// Hook for Wii U Menu process
// -----------------------------------------------------------------------------
// We need a separate hook because WUPS requires unique function names
// for each process target.
// -----------------------------------------------------------------------------

DECL_FUNCTION(int32_t, VPADRead_Menu, VPADChan chan, VPADStatus* buffer,
              uint32_t bufferSize, VPADReadError* error)
{
    VPADReadError realError = VPAD_READ_UNINITIALIZED;
    int32_t result = real_VPADRead_Menu(chan, buffer, bufferSize, &realError);

    if (result > 0 && realError == VPAD_READ_SUCCESS) {
        handleInput(buffer, bufferSize);
    }

    if (error) {
        *error = realError;
    }

    return result;
}

// -----------------------------------------------------------------------------
// Register the hooks
// -----------------------------------------------------------------------------
// WUPS_MUST_REPLACE_FOR_PROCESS tells WUPS to:
// - Replace the VPADRead function with our hook
// - Only do this for the specified process type
//
// WUPS_FP_TARGET_PROCESS_GAME: Applies to games
// WUPS_FP_TARGET_PROCESS_WII_U_MENU: Applies to the Wii U home menu
// -----------------------------------------------------------------------------

WUPS_MUST_REPLACE_FOR_PROCESS(
    VPADRead_Game,                    // Our hook function
    WUPS_LOADER_LIBRARY_VPAD,         // Library containing VPADRead
    VPADRead,                         // Original function name
    WUPS_FP_TARGET_PROCESS_GAME       // Target process type
);

WUPS_MUST_REPLACE_FOR_PROCESS(
    VPADRead_Menu,
    WUPS_LOADER_LIBRARY_VPAD,
    VPADRead,
    WUPS_FP_TARGET_PROCESS_WII_U_MENU
);
