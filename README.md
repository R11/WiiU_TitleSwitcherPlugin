# Title Switcher Plugin for Wii U

A plugin for the Aroma homebrew environment that provides a quick game launcher menu accessible via button combo. Switch between installed titles without returning to the slow Wii U system menu.

## Features

- Quick overlay menu accessible from any app via **L + Minus + D-Pad Right**
- Browse all installed games and apps
- Favorites system for quick access to your most-played titles
- User-defined categories for custom organization
- System Apps category (including vWii Mode)
- Multi-screen support (GamePad, TV 1080p/720p/480p)
- Persistent settings saved to SD card

## Installation

1. Copy `TitleSwitcherPlugin.wps` to your SD card:
   ```
   sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps
   ```
2. Boot your Wii U with Aroma

## Usage

While in any application, press **L + Minus + D-Pad Right** to open the Title Switcher menu.

### Controls

| Button | Action |
|--------|--------|
| **L + - + Right** | Open/Close menu |
| D-Pad Up/Down | Navigate list |
| D-Pad Left/Right | Skip 5 items |
| L/R | Skip 15 items (page) |
| A | Launch selected title |
| B | Back / Exit menu |
| Y | Toggle favorite |
| X | Edit mode |
| + | Open settings |
| ZL/ZR | Previous/Next category |

### Categories

- **All** - All installed titles
- **Favorites** - Your favorited titles (toggle with Y)
- **Custom** - Create your own categories in Edit mode

## Building

Requires Docker to build:

```bash
./build.sh
```

Or with devkitPro installed locally:

```bash
make -j$(nproc)
```

### Running Tests

```bash
cd tests && make && ./run_tests
```

### Preview Tool

ASCII renderer for validating UI layouts without hardware:

```bash
cd tools/preview && make && ./preview_demo
```

## Technical Details

- Integrates with WUPS for plugin framework and storage
- Uses `MCP_TitleListByAppType` to enumerate games and system apps
- Uses `ACPGetTitleMetaXml` to retrieve title names
- Uses `SYSLaunchTitle` to switch titles
- VPADRead interception for button combo detection
- OSScreen backend for rendering overlay

## Requirements

- Wii U console with Aroma homebrew environment
- Notification Module (for status messages)

## License

GPLv3

## Author

R11
