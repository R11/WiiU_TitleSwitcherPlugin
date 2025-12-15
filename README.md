# Title Switcher Plugin for Wii U

A plugin for the Aroma homebrew environment that allows you to switch between games directly from the Aroma config menu without returning to the slow Wii U Menu.

## Features

- Quick game switching from Aroma's config menu
- Lists all installed Wii U games alphabetically
- Single-item browser with page navigation
- Excludes currently running game from the list

## Installation

1. Copy `TitleSwitcherPlugin.wps` to your SD card:
   ```
   sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps
   ```
2. Boot your Wii U with Aroma

## Usage

1. While in any application, press **L + D-Pad Down + Minus** to open Aroma's config menu
2. Select **Title Switcher** from the plugin list
3. Use the controls to browse and select a game:

| Button | Action |
|--------|--------|
| D-Pad Left/Right | Navigate ±1 game |
| L / R | Page jump ±10 games |
| A | Launch selected game |
| B | Back / Exit menu |

The display shows: `GameName (X/Y)` where X is current position and Y is total games.

## Building

Requires Docker to build:

```bash
./build.sh
```

Or manually:

```bash
docker build -t titleswitcherplugin .
docker run --rm -v "$(pwd):/output" titleswitcherplugin cp /project/TitleSwitcherPlugin.wps /output/
```

## Technical Details

- Integrates with WUPS Config API for menu interface
- Uses `MCP_TitleListByAppType` to enumerate installed games
- Uses `ACPGetTitleMetaXml` to retrieve game names
- Uses `SYSLaunchTitle` to switch titles

## Requirements

- Wii U console with Aroma homebrew environment
- Notification Module (for status messages)

## License

GPLv3

## Author

R11

## Version History

- 0.8.0 - Single-item browser with L/R pagination
- 0.7.0 - Config menu integration (full list)
- 0.1.0 - Initial release
