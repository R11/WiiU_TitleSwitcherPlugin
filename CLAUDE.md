# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Build the plugin (requires Docker):
```bash
./build.sh
```

This builds inside a Docker container with devkitPro/devkitPPC and outputs `TitleSwitcherPlugin.wps`.

For debug builds, modify the Dockerfile to pass `DEBUG=1` to make.

## Project Overview

TitleSwitcherPlugin is a Wii U Plugin System (WUPS) plugin for the Aroma homebrew environment. It provides a game launcher menu accessible via button combo (L+R+Minus) that allows switching between installed games without returning to the Wii U Menu.

## Architecture

### Entry Point and Plugin Lifecycle (`src/main.cpp`)
- Defines WUPS plugin metadata and lifecycle hooks
- Hooks `VPADRead` to intercept GamePad input for combo detection
- Separate hooks for game processes (`VPADRead_Game`) and Wii U Menu (`VPADRead_Menu`)
- Application lifecycle hooks (`ON_APPLICATION_START`, `ON_APPLICATION_ENDS`, etc.) track state for safe menu opening

### Core Modules

**Menu System (`src/menu/`)**
- `menu.cpp/h`: Main menu loop, rendering, input handling
- `categories.cpp/h`: Category filtering and display (All, Favorites, user-defined)
- Split-screen layout: title list (left 30%), details panel (right 70%)
- Modes: BROWSE (normal), EDIT (modify title categories), SETTINGS

**Title Management (`src/titles/`)**
- `titles.cpp/h`: Enumerates installed titles via `MCP_TitleListByAppType`
- Retrieves names via `ACPGetTitleMetaXml`
- Caches title list in memory (enumeration is slow)
- Filters out currently running title

**Rendering (`src/render/`)**
- `renderer.cpp/h`: Abstract renderer interface (OSScreen backend currently)
- `image_loader.cpp/h`: PNG/JPEG loading via libgd
- Dynamic layout calculations for DRC vs TV screen sizes

**Storage (`src/storage/`)**
- `settings.cpp/h`: WUPS Storage API wrapper
- Saves to `sd:/wiiu/plugins/config/TitleSwitcher.json`
- Stores favorites, custom categories, colors, last selection

**Presets (`src/presets/`)**
- `title_presets.cpp/h`: GameTDB metadata integration
- Loads from `sd:/wiiu/plugins/config/TitleSwitcher_presets.json`
- Provides publisher/developer/genre/region data for category suggestions
- Use `tools/convert_gametdb.py` to convert GameTDB XML to JSON

**Input (`src/input/`)**
- `buttons.h`: Centralized button mapping (change mappings in one place)
- `text_input.cpp/h`: On-screen text input for category names

### Tools (`tools/`)
- `convert_gametdb.py`: Converts GameTDB XML to presets JSON
- `preview/`: Desktop preview tool for testing menu rendering (uses stubs)

## Key APIs Used

- **WUPS**: Plugin framework, function hooking, storage API
- **WUT**: Wii U Toolchain (VPADRead, OSScreen, MCP, ACP, SYSLaunchTitle)
- **libnotifications**: Toast notifications
- **libmappedmemory**: Memory allocation for large buffers
- **libgd**: Image loading (PNG, JPEG)

## Code Conventions

- C++20 standard
- Namespaces match module names (e.g., `Menu::`, `Titles::`, `Renderer::`)
- Header files contain extensive documentation with usage examples
- Colors in RGBA format: `0xRRGGBBAA`
- Title IDs are 64-bit (`uint64_t`)
