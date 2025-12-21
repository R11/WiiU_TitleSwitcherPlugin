/**
 * Internal Menu State
 *
 * Shared state and types between menu panels.
 * Not part of public API - internal use only.
 */

#pragma once

#include "menu.h"
#include "../ui/list_view.h"
#include "../input/text_input.h"
#include "../storage/settings.h"
#include <coreinit/time.h>
#include <cstdint>

namespace Menu {
namespace Internal {

// =============================================================================
// Settings System Types
// =============================================================================

enum class SettingsSubMode {
    MAIN,
    MANAGE_CATS,
    SYSTEM_APPS,
    COLORS,
    COLOR_INPUT,
    NAME_INPUT
};

enum class SettingType {
    TOGGLE,
    COLOR,
    BRIGHTNESS,
    ACTION
};

enum SettingAction {
    ACTION_MANAGE_CATEGORIES = -1,
    ACTION_SYSTEM_APPS = -2,
    ACTION_DEBUG_GRID = -3,
    ACTION_COLORS = -4
};

struct ColorOption {
    const char* name;
    int dataOffset;
};

struct SettingItem {
    const char* name;
    const char* descLine1;
    const char* descLine2;
    SettingType type;
    int dataOffset;
};

#define TOGGLE_SETTING(n, d1, d2, member) \
    {n, d1, d2, SettingType::TOGGLE, (int)offsetof(Settings::PluginSettings, member)}
#define COLOR_SETTING(n, d1, d2, member) \
    {n, d1, d2, SettingType::COLOR, (int)offsetof(Settings::PluginSettings, member)}
#define ACTION_SETTING(n, d1, d2, actionId) \
    {n, d1, d2, SettingType::ACTION, actionId}

#define COLOR_OPTION(n, member) \
    {n, (int)offsetof(Settings::PluginSettings, member)}

// =============================================================================
// System Apps Types
// =============================================================================

struct SystemAppOption {
    const char* name;
    const char* description;
    int appId;
};

constexpr int SYSAPP_RETURN_TO_MENU = -1;
constexpr int SYSAPP_BROWSER = -2;
constexpr int SYSAPP_ESHOP = -3;
constexpr int SYSAPP_CONTROLLER_SYNC = -4;

// =============================================================================
// State Variables (defined in menu.cpp)
// =============================================================================

extern bool sIsOpen;
extern bool sInitialized;
extern Mode sCurrentMode;

extern UI::ListView::State sTitleListState;
extern UI::ListView::State sEditCatsListState;
extern UI::ListView::State sSettingsListState;
extern UI::ListView::State sManageCatsListState;
extern UI::ListView::State sSystemAppsListState;
extern UI::ListView::State sColorsListState;

extern SettingsSubMode sSettingsSubMode;
extern SettingsSubMode sColorReturnSubmode;
extern int sEditingSettingIndex;
extern int sEditingColorOffset;
extern int sEditingCategoryId;
extern TextInput::Field sInputField;

extern OSTime sApplicationStartTime;
extern bool sInForeground;
extern bool sOpeningInProgress;
extern const uint32_t STARTUP_GRACE_MS;

extern const SettingItem sSettingItems[];
extern const int SETTINGS_ITEM_COUNT;

extern const SystemAppOption sSystemApps[];
extern const int SYSTEM_APP_COUNT;

extern const ColorOption sColorOptions[];
extern const int COLOR_OPTION_COUNT;

// =============================================================================
// Shared Helper Functions
// =============================================================================

bool* getTogglePtr(int offset);
uint32_t* getColorPtr(int offset);
void clampSelection();
void drawHeaderDivider();
void drawDetailsPanelSectionHeader(const char* title, bool shortUnderline = false);
const char* getSettingActionHint(SettingType type);
inline bool isValidSelection(int index, int count) { return index >= 0 && index < count; }

}
}
