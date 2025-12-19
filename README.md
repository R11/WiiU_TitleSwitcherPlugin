# Title Switcher Plugin

A Wii U Aroma plugin that lets you switch between games directly from an in-game menu, without returning to the slow Wii U Menu.

## Features

- **Quick Launch Menu** - Press L+R+Minus to open from any game
- **Categories** - All, Favorites, Games, System Apps, plus custom categories
- **Favorites** - Mark games as favorites for quick access
- **Game Metadata** - Shows publisher, developer, genre, and release info from GameTDB
- **Customizable** - Change background color and other settings

## Installation

Extract `TitleSwitcherPlugin.zip` to the root of your SD card:

```
sd:/wiiu/environments/aroma/plugins/TitleSwitcherPlugin.wps
sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher_presets.json
```

## Controls

| Button | Action |
|--------|--------|
| **L+R+Minus** | Open menu |
| **Up/Down** | Navigate list |
| **Left/Right** | Skip 5 items |
| **L/R** | Page up/down |
| **ZL/ZR** | Switch category |
| **A** | Launch game |
| **B** | Back / Close menu |
| **Y** | Toggle favorite |
| **X** | Edit categories |
| **+** | Settings |

## Categories

- **All** - All installed titles
- **Favorites** - Your marked favorites
- **Games** - Retail and eShop games
- **System** - System apps (Browser, eShop, Mii Maker, etc.)
- **Custom** - Create your own categories via X button

## Building

```bash
./build.sh
```

Requires Docker. Output: `TitleSwitcherPlugin.wps`

## Requirements

- Wii U with Aroma homebrew environment
- Notification Module (optional, for status messages)

## License

GPLv3

## Author

R11
