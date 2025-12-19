# CLAUDE.md

This file provides guidance for AI assistants working on this codebase.

## Project Overview

**Title Switcher Plugin** is a Wii U homebrew plugin for the Aroma environment that provides a quick game launcher menu accessible via button combo (L + R + Minus). It allows users to switch between installed titles without returning to the slow Wii U system menu.

## Build System

### Docker Build (Recommended)
```bash
./build.sh
```
This builds in a container with devkitPro/devkitPPC and produces `TitleSwitcherPlugin.wps`.

### Direct Build (Requires devkitPro)
```bash
make -j$(nproc)
```

### Running Tests
```bash
cd tests && make && ./run_tests
```
Tests use Google Test framework with mocked WUPS/WUT APIs.

### Preview Tool
```bash
cd tools/preview && make && ./preview_demo
```
ASCII renderer for validating UI layouts without hardware. Options: `--screen drc|tv1080|tv720|tv480`, `--no-color`, `--compact`.

## Architecture

```
src/
├── main.cpp           # Entry point, WUPS hooks, VPADRead interception
├── input/
│   ├── buttons.h      # Button mappings (constexpr, header-only)
│   └── text_input.*   # Text input field for naming
├── render/
│   ├── renderer.*     # Abstract renderer (OSScreen backend)
│   └── image_loader.* # PNG/JPG loading via libgd
├── menu/
│   ├── menu.*         # Main state machine and loop
│   └── categories.*   # Category filtering (All, Favorites, System, custom)
├── titles/
│   └── titles.*       # Title enumeration via MCP/ACP APIs
├── presets/
│   └── title_presets.*# GameTDB metadata integration
├── storage/
│   └── settings.*     # Persistent storage via WUPS Storage API
└── utils/
    └── dc.h           # Display Controller register save/restore
```

## Key Patterns

### Namespace Organization
- `Buttons::` - Button definitions and combo detection
- `Menu::` - Menu state and rendering
- `Renderer::` - Screen abstraction layer
- `Titles::` - Title management
- `Settings::` - Persistent configuration
- `Categories::` - Filtering logic
- `TitlePresets::` - GameTDB preset metadata

### Button System
All buttons defined in `src/input/buttons.h` as constexpr. Use semantic names like `Buttons::CONFIRM`, `Buttons::LAUNCH` rather than raw VPAD constants.

### Multi-Screen Support
Dynamic layouts support: DRC GamePad (854x480), TV 1080p/720p/480p. Layout calculations in `menu.cpp` use screen dimensions for responsive positioning.

### Safety Systems
- **Grace period**: 5-second delay after app start before menu can open
- **Foreground tracking**: Menu disabled during app transitions
- **ProcUI state checking**: Ensures stable app state
- **DC register save/restore**: Clean graphics takeover

### Storage Format
WUPS Storage API writes to `sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher.json`. Version-based migration (CONFIG_VERSION = 2).

### Presets System
GameTDB metadata loaded from `sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher_presets.json`. Provides publisher, developer, release date, genre, and region data. Use `tools/convert_gametdb.py` to generate from GameTDB XML.

## Common Tasks

### Adding a New Button Action
1. Add constexpr in `src/input/buttons.h`
2. Handle in `Menu::handleInput()` in `menu.cpp`

### Adding a Settings Field
1. Add to `Settings` struct in `settings.h`
2. Add load/save in `loadSettings()`/`saveSettings()`
3. Increment CONFIG_VERSION if migration needed

### Adding a Category
Built-in categories defined in `categories.cpp`. User categories stored in settings.

## Git Hooks

Install the pre-commit hook to automatically run tests and update snapshots:
```bash
./scripts/install-hooks.sh
```

The pre-commit hook will:
- Run unit tests (abort commit if they fail)
- Regenerate snapshots and stage any changes

To skip temporarily: `git commit --no-verify`

## Testing Changes

1. Run unit tests: `cd tests && make && ./run_tests`
2. Run preview tool: `cd tools/preview && make && ./preview_demo`
3. Verify snapshots: `cd tools/preview && ./preview_demo --verify-snapshots`
4. Build plugin: `./build.sh`
5. Test on hardware or Cemu

## Dependencies

- **WUPS (Wii U Plugin System)** - Plugin framework
- **WUT (Wii U Toolchain)** - System APIs
- **libmappedmemory** - GX2-compatible memory
- **libnotifications** - In-game notifications
- **libgd, libpng, libjpeg** - Image loading

## Code Style

### Comments
- Keep comments to 1-2 lines maximum
- Never put comments on the same line as code
- Prefer descriptive variable/function names over comments
- Delete comments that restate what code already expresses
- No ASCII art diagrams or lengthy documentation blocks in source files

### Variable Naming
- Use verbose, self-explanatory names (e.g., `categoryIndex` not `catIdx`)
- Iterator names should describe what they iterate (e.g., `requestIterator` not `it`)
- Boolean variables should read as questions (e.g., `isVisible`, `hasLoaded`)

## Color Palette

Uses Catppuccin Mocha theme. Constants defined in `settings.h`:
- `DEFAULT_BG_COLOR` = 0x1E1E2EFF (Base)
- `DEFAULT_TITLE_COLOR` = 0xCDD6F4FF (Text)
- `DEFAULT_HIGHLIGHTED_COLOR` = 0x89B4FAFF (Blue)
