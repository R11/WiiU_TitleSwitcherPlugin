# WUPS Config API Reference for Title Switcher

This document covers the WUPS (Wii U Plugin System) Config API used for building plugin menus in Aroma.

## Official Resources

- **WUPS GitHub**: https://github.com/wiiu-env/WiiUPluginSystem
- **WUPS Headers**: https://github.com/wiiu-env/WiiUPluginSystem/tree/main/include/wups
- **Example Plugins**:
  - https://github.com/Lynx64/CloseRestartGamePlugin
  - https://github.com/wiiu-env/AromaBasePlugin

## Config API Overview

The config menu is accessed via **L + D-Pad Down + Minus** while in any application.

### Initialization

```cpp
#include <wups/config_api.h>

// In INITIALIZE_PLUGIN():
WUPSConfigAPIOptionsV1 configOptions = {
    .name = "Your Plugin Name"
};

WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);
```

### Menu Structure

The menu has a hierarchical structure:
- **Root Category** (passed to your callback)
  - **Subcategories** (optional nesting)
    - **Items** (the interactive elements)

## Callback Flow

```
User opens config menu
    → ConfigMenuOpenedCallback(rootHandle)
        → You create categories/items here
        → Return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS

User interacts with menu
    → Item callbacks fire (onInput, getCurrentValueDisplay, etc.)

User exits menu (B button)
    → ConfigMenuClosedCallback()
        → Good place to trigger deferred actions (like launching a game)
```

## Creating Categories

```cpp
WUPSConfigCategoryHandle subCategory;
WUPSConfigAPI_Category_Create({.name = "Category Name"}, &subCategory);
WUPSConfigAPI_Category_AddCategory(rootHandle, subCategory);

// Add items to subcategory instead of root
WUPSConfigAPI_Category_AddItem(subCategory, itemHandle);
```

## Creating Items (V2 API)

```cpp
WUPSConfigAPIItemOptionsV2 itemOptions = {
    .displayName = "Item Label",      // Left side of the menu row
    .context = yourDataPointer,       // Passed to all callbacks
    .callbacks = {
        .getCurrentValueDisplay = ...,
        .getCurrentValueSelectedDisplay = ...,
        .onSelected = ...,
        .restoreDefault = ...,
        .isMovementAllowed = ...,
        .onCloseCallback = ...,
        .onInput = ...,
        .onInputEx = ...,
        .onDelete = ...,
    }
};

WUPSConfigItemHandle itemHandle;
WUPSConfigAPI_Item_Create(itemOptions, &itemHandle);
WUPSConfigAPI_Category_AddItem(rootHandle, itemHandle);
```

## Item Callbacks Explained

### getCurrentValueDisplay / getCurrentValueSelectedDisplay
```cpp
static int32_t GetDisplayValue(void* context, char* buf, int32_t bufSize)
{
    // Set the text shown on the RIGHT side of the menu item
    snprintf(buf, bufSize, "Current Value");
    return 0;
}
```
- Called when the menu needs to render the item's value
- `getCurrentValueDisplay` = when item is NOT selected (cursor elsewhere)
- `getCurrentValueSelectedDisplay` = when item IS selected (cursor on it)
- **NOTE**: May be cached by the menu - might not update immediately when you change state

### onSelected
```cpp
static void OnSelected(void* context, bool isSelected)
{
    // isSelected = true when cursor moves TO this item
    // isSelected = false when cursor moves AWAY from this item
}
```

### onInput
```cpp
static void OnInput(void* context, WUPSConfigSimplePadData input)
{
    // Called every frame while this item is selected
    // input.buttons_d = buttons just pressed this frame
    // input.buttons_r = buttons just released this frame
    // input.buttons_h = buttons currently held

    if (input.buttons_d & WUPS_CONFIG_BUTTON_A) { /* A pressed */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_B) { /* B pressed */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_L) { /* L pressed */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_R) { /* R pressed */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_LEFT) { /* D-pad left */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_RIGHT) { /* D-pad right */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_UP) { /* D-pad up */ }
    if (input.buttons_d & WUPS_CONFIG_BUTTON_DOWN) { /* D-pad down */ }
}
```

### isMovementAllowed
```cpp
static bool IsMovementAllowed(void* context)
{
    // Return false to "trap" the cursor on this item
    // (prevents UP/DOWN from moving to other items)
    return true;
}
```

### restoreDefault
```cpp
static void RestoreDefault(void* context)
{
    // Called when user triggers "restore defaults"
}
```

### onCloseCallback
```cpp
static void OnClose(void* context)
{
    // Called when config menu is about to close
    // Per-item cleanup
}
```

### onDelete
```cpp
static void OnDelete(void* context)
{
    // Called when item is being destroyed
    // Free any allocated context memory here
    if (context) free(context);
}
```

## Available Button Constants

```cpp
WUPS_CONFIG_BUTTON_A
WUPS_CONFIG_BUTTON_B
WUPS_CONFIG_BUTTON_X
WUPS_CONFIG_BUTTON_Y
WUPS_CONFIG_BUTTON_L
WUPS_CONFIG_BUTTON_R
WUPS_CONFIG_BUTTON_ZL
WUPS_CONFIG_BUTTON_ZR
WUPS_CONFIG_BUTTON_UP
WUPS_CONFIG_BUTTON_DOWN
WUPS_CONFIG_BUTTON_LEFT
WUPS_CONFIG_BUTTON_RIGHT
WUPS_CONFIG_BUTTON_PLUS
WUPS_CONFIG_BUTTON_MINUS
WUPS_CONFIG_BUTTON_STICK_L
WUPS_CONFIG_BUTTON_STICK_R
```

## Built-in Item Types

WUPS provides pre-built item types in `<wups/config/...>`:

### WUPSConfigItemBoolean
Toggle on/off with callback when changed.

### WUPSConfigItemIntegerRange
Numeric slider with min/max.

### WUPSConfigItemMultipleValues
Dropdown-style selector. User presses LEFT/RIGHT to cycle through options.

```cpp
#include <wups/config/WUPSConfigItemMultipleValues.h>

ConfigItemMultipleValuesPair values[] = {
    {0, "Option A"},
    {1, "Option B"},
    {2, "Option C"},
};

WUPSConfigItemMultipleValues::AddToCategory(
    categoryHandle,
    "setting_id",
    "Display Name",
    0,  // default index
    0,  // current index
    values,
    3,  // count
    [](uint32_t newValue) {
        // Called when value changes
    }
);
```

## Known Limitations & Issues

### No Overlay API
The overlay functions (`WUPS_OpenOverlay`, `WUPS_Overlay_*`) mentioned in old documentation **do not exist** in current WUPS. Custom screen rendering is not supported.

### Display Caching
The config menu may cache `getCurrentValueDisplay` results. Changing internal state (like `sSelectedIndex`) may not immediately update the display. The display seems to refresh when:
- Cursor moves to/from the item
- Some input is processed
- Menu is redrawn for other reasons

### No Programmatic Menu Close
There's no API to programmatically close the config menu from within a callback. You cannot force the menu to exit when user presses A.

### Launch Timing
Games cannot be launched while the config menu is open. The pattern is:
1. User presses A → set a "pending launch" flag
2. User presses B to exit menu
3. `ConfigMenuClosedCallback` fires → call `SYSLaunchTitle()`

**Issue with single-item browser**: Unlike multiple items where pressing A might trigger some menu behavior, a single item just sits there. The user must manually press B to exit and trigger the launch.

## Architecture Options for Title Browser

### Option A: Single Browser Item (Current)
- One item, LEFT/RIGHT cycles through games
- L/R for page jumps
- A to select, B to exit and launch
- **Pros**: Compact, fast navigation
- **Cons**: Only shows one game at a time, A doesn't auto-exit

### Option B: Multiple Items (Previous Version)
- One item per game in the list
- UP/DOWN to navigate (menu's native behavior)
- A to select and launch
- **Pros**: See multiple games, native navigation feel
- **Cons**: Slow with many games, no pagination

### Option C: Category-Based Organization
- Create subcategories: "A-F", "G-L", etc. or "Wii U", "Wii", etc.
- Each category has its games as items
- **Pros**: Organized, manageable list sizes
- **Cons**: More navigation to find games

### Option D: WUPSConfigItemMultipleValues
- Use the built-in multiple values item type
- Native LEFT/RIGHT cycling through games
- **Pros**: Built-in, guaranteed to work
- **Cons**: Same single-item display as Option A

## Example: Category-Based Structure

```cpp
static WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
    // Create letter-based categories
    const char* letterGroups[] = {"A-F", "G-L", "M-R", "S-Z"};

    for (int g = 0; g < 4; g++) {
        WUPSConfigCategoryHandle letterCat;
        WUPSConfigAPI_Category_Create({.name = letterGroups[g]}, &letterCat);
        WUPSConfigAPI_Category_AddCategory(rootHandle, letterCat);

        // Add games that start with these letters
        for (int i = 0; i < sTitleCount; i++) {
            char firstLetter = toupper(sTitles[i].name[0]);
            bool inGroup = false;

            switch (g) {
                case 0: inGroup = (firstLetter >= 'A' && firstLetter <= 'F'); break;
                case 1: inGroup = (firstLetter >= 'G' && firstLetter <= 'L'); break;
                case 2: inGroup = (firstLetter >= 'M' && firstLetter <= 'R'); break;
                case 3: inGroup = (firstLetter >= 'S' && firstLetter <= 'Z'); break;
            }

            if (inGroup) {
                // Create and add item for this game to letterCat
            }
        }
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}
```

## Useful WUT/System APIs

### Title Operations
```cpp
#include <coreinit/title.h>
#include <coreinit/mcp.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>

// Get current running title
uint64_t currentTitle = OSGetTitleID();

// Enumerate games
int32_t mcpHandle = MCP_Open();
MCPTitleListType* list = malloc(...);
uint32_t count;
MCP_TitleListByAppType(mcpHandle, MCP_APP_TYPE_GAME, &count, list, size);
MCP_Close(mcpHandle);

// Get game name
ACPMetaXml* meta = memalign(0x40, sizeof(ACPMetaXml));
ACPGetTitleMetaXml(titleId, meta);
// Use meta->shortname_en, meta->longname_en, etc.

// Launch a game
SYSLaunchTitle(titleId);
```

### Notifications
```cpp
#include <notifications/notifications.h>

NotificationModule_InitLibrary();
NotificationModule_AddInfoNotification("Message");
NotificationModule_DeInitLibrary();
```
