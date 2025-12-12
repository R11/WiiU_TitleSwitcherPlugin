# Title Switcher Plugin for Wii U

A plugin for the Aroma homebrew environment that allows you to switch between games directly from the HOME button menu without returning to the slow Wii U Menu.

## Features

- Quick game switching from the HOME menu overlay
- Lists all installed Wii U games alphabetically
- Simple text-based UI on both TV and GamePad
- Page navigation for large game libraries

## Installation

1. Copy `TitleSwitcherPlugin.wps` to your SD card:
   ```
   sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps
   ```
2. Boot your Wii U with Aroma

## Usage

1. While playing any game, press the **HOME** button to open the HOME menu
2. Press **L + R + MINUS (SELECT)** simultaneously to open Title Switcher
3. Use the controls to navigate and select a game:

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Navigate list |
| L / R | Page up/down |
| A | Launch selected game |
| B | Cancel and close menu |

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

- Hooks `VPADRead` in the HOME Menu process to detect button combos
- Uses `MCP_TitleListByAppType` to enumerate installed games
- Uses `ACPGetTitleMetaXml` to retrieve game names
- Uses `OSScreen` for text-based rendering
- Uses `SYSLaunchTitle` to switch titles

## Requirements

- Wii U console with Aroma homebrew environment
- Notification Module (for status messages)

## License

GPLv3

## Author

Rudger

## Version History

- 0.1.0 - Initial release
