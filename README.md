# Title Switcher Plugin for Wii U

A plugin for the Aroma homebrew environment that allows you to switch between games and apps directly from the Aroma config menu without returning to the slow Wii U Menu.

## Features

- Quick game/app switching from Aroma's config menu
- Games organized alphabetically in categories: A-F, G-L, M-R, S-Z, # (numbers)
- System Apps in separate category (including vWii Mode)
- Excludes currently running title from the list
- Press A to launch immediately

## Installation

1. Copy `TitleSwitcherPlugin.wps` to your SD card:
   ```
   sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps
   ```
2. Boot your Wii U with Aroma

## Usage

1. While in any application, press **L + D-Pad Down + Minus** to open Aroma's config menu
2. Select **Title Switcher** from the plugin list
3. Navigate to a category (A-F, G-L, etc. or System Apps)
4. Select a game and press **A** to launch

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate list |
| A | Launch selected title |
| B | Back / Exit menu |

## Categories

- **A-F, G-L, M-R, S-Z** - Games sorted alphabetically
- **# (0-9)** - Games starting with numbers/symbols
- **System Apps** - System applications and vWii Mode

## Building

Requires Docker to build:

```bash
./build.sh
```

## Technical Details

- Integrates with WUPS Config API for menu interface
- Uses `MCP_TitleListByAppType` to enumerate games and system apps
- Uses `ACPGetTitleMetaXml` to retrieve title names
- Uses `SYSLaunchTitle` to switch titles

## Requirements

- Wii U console with Aroma homebrew environment
- Notification Module (for status messages)

## License

GPLv3

## Author

R11

## Version History

- 1.0.0 - Category-based browser with System Apps support
- 0.8.0 - Single-item browser with L/R pagination
- 0.7.0 - Config menu integration (full list)
- 0.1.0 - Initial release
