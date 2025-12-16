# Title Switcher Plugin - Architecture

## Overview

Title Switcher is a Wii U Plugin System (WUPS) plugin for the Aroma custom firmware.
It provides a custom game launcher menu accessible via button combo (L + R + Minus) that
renders directly to the screen using OSScreen, bypassing the normal game rendering.

## Key Technologies

### WUPS (Wii U Plugin System)
- **Repository**: https://github.com/wiiu-env/WiiUPluginSystem
- **Documentation**: https://wiiu-env.github.io/WiiUPluginSystem/
- **Purpose**: Framework for creating plugins that hook into Wii U system functions
- **Key Features Used**:
  - Function hooking (VPADRead for input interception)
  - Storage API (persistent settings)
  - Application lifecycle hooks (ON_APPLICATION_START, etc.)

### WUT (Wii U Toolchain)
- **Repository**: https://github.com/devkitPro/wut
- **Documentation**: https://wut.devkitpro.org/
- **Purpose**: Low-level Wii U SDK providing access to system APIs
- **Key APIs Used**:
  - `coreinit/screen.h` - OSScreen framebuffer rendering
  - `coreinit/mcp.h` - Title list enumeration (MCP_TitleListByAppType)
  - `coreinit/time.h` - Timing functions
  - `nn/acp/title.h` - Title metadata (ACPGetTitleMetaXml)
  - `vpad/input.h` - GamePad input reading
  - `sysapp/launch.h` - Title launching (SYSLaunchTitle)
  - `proc_ui/procui.h` - Process UI state management
  - `gx2/event.h` - Graphics synchronization (GX2WaitForVsync)

### libmappedmemory
- **Repository**: https://github.com/wiiu-env/libmappedmemory
- **Purpose**: Allocate memory compatible with GX2 graphics system
- **Why Needed**: Regular malloc'd memory causes graphical corruption when used
  with OSScreen because it's not properly mapped for the GPU

## Module Structure

```
src/
├── main.cpp              # Plugin entry point, WUPS hooks, VPADRead hooks
│
├── input/
│   └── buttons.h         # Button mapping constants and labels
│                         # Centralizes all button definitions for easy remapping
│
├── render/
│   ├── screen.h          # OSScreen wrapper interface
│   └── screen.cpp        # Screen initialization, text rendering, cleanup
│                         # Handles DC register save/restore for clean takeover
│
├── titles/
│   ├── titles.h          # Title management interface
│   └── titles.cpp        # Title enumeration, caching, metadata lookup
│                         # Uses MCP and ACP APIs to get installed titles
│
├── storage/
│   ├── settings.h        # Settings structure and interface
│   └── settings.cpp      # WUPS Storage API implementation
│                         # Persists favorites, categories, colors, etc.
│
├── menu/
│   ├── menu.h            # Main menu interface
│   ├── menu.cpp          # Menu state machine, main loop, rendering
│   ├── categories.h      # Category system interface
│   └── categories.cpp    # Category management, filtering
│
└── utils/
    └── dc.h              # Display Controller register access
                          # Low-level graphics state save/restore
```

## Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              WUPS Plugin Loader                              │
│  Loads plugin, calls INITIALIZE_PLUGIN, installs function hooks             │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                               main.cpp                                       │
│  - INITIALIZE_PLUGIN: Init subsystems, preload titles                       │
│  - VPADRead hooks: Intercept input, detect button combo                     │
│  - ON_APPLICATION_START/END: Track app lifecycle for safety                 │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                          Button combo detected
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            menu/menu.cpp                                     │
│  Menu::Open() called:                                                        │
│  1. Screen::Init() - Take over display                                       │
│  2. Run menu loop (render, handle input)                                     │
│  3. Screen::Shutdown() - Restore display to game                            │
│  4. If title selected: SYSLaunchTitle()                                     │
└─────────────────────────────────────────────────────────────────────────────┘
         │                      │                        │
         ▼                      ▼                        ▼
┌─────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐
│  render/screen  │  │   titles/titles     │  │  storage/settings   │
│                 │  │                     │  │                     │
│ - OSScreen APIs │  │ - MCP title list    │  │ - WUPS Storage API  │
│ - DC registers  │  │ - ACP metadata      │  │ - Favorites         │
│ - Text drawing  │  │ - Sorting/filtering │  │ - Categories        │
└─────────────────┘  └─────────────────────┘  └─────────────────────┘
```

## Screen Rendering Pipeline

The plugin takes over screen rendering when the menu opens. This is a delicate
process that must properly save and restore the game's graphics state.

### Opening the Menu

1. **Save DC Registers** - The Display Controller registers control how the
   framebuffer is displayed. We save 8 registers (4 per screen) so we can
   restore them later.

2. **OSScreenInit()** - Initialize the OSScreen subsystem for text rendering

3. **Allocate Buffers** - Use MEMAllocFromMappedMemoryForGX2Ex() to allocate
   framebuffers that are compatible with the GX2 graphics system

4. **Double Clear+Flip** - Clear and flip buffers twice to ensure both the
   front and back buffers are initialized (prevents garbage on first frame)

5. **Disable Home Button** - Prevent HOME menu from interfering while our
   menu is open

### Menu Loop

1. **GX2WaitForVsync()** - Wait for vertical blank to prevent tearing
2. **Clear buffers** - Fill with background color
3. **Draw content** - Text, selection cursor, categories, etc.
4. **DCFlushRange()** - Flush CPU cache to ensure GPU sees updates
5. **OSScreenFlipBuffersEx()** - Swap front/back buffers

### Closing the Menu

1. **Restore DC Registers** - Put graphics back to game's state
2. **Free Buffers** - Return memory to system
3. **Re-enable Home Button** - Restore normal behavior

## Safety Systems

### Startup Grace Period
Prevents menu from opening during the first few seconds after a game starts,
when the game may still be initializing graphics and loading resources.

### Foreground Tracking
Uses WUPS lifecycle hooks (ON_ACQUIRED_FOREGROUND, ON_RELEASE_FOREGROUND) to
track whether the app is in foreground. Menu won't open during transitions.

### ProcUI State Check
Verifies ProcUIIsRunning() before opening menu. This catches cases where the
app is shutting down or in an unstable state.

## Storage Format

Settings are stored using the WUPS Storage API, which persists data to SD card.

```
Storage Key          Type        Description
─────────────────────────────────────────────────────────────
configVersion        int32       Schema version for migrations
lastIndex            int32       Last selected title index
bgColor              uint32      Background color (RGBA)
titleColor           uint32      Normal title text color
highlightedColor     uint32      Selected title color
favoriteColor        uint32      Favorite marker color
headerColor          uint32      Header text color
favoritesCount       int32       Number of favorites
favoritesData        binary      Array of favorite title IDs (uint64[])
categoriesCount      int32       Number of custom categories
categoriesData       binary      Serialized category data
titleCategoriesData  binary      Title-to-category assignments
```

## Attribution

This project incorporates patterns and code from:

### WiiUPluginLoaderBackend (GPLv3)
- **Source**: https://github.com/wiiu-env/WiiUPluginLoaderBackend
- **Author**: Maschell
- **Used For**: Display Controller register access pattern, OSScreen initialization
  sequence, MEMAllocFromMappedMemoryForGX2Ex usage pattern
- **Files Affected**: src/utils/dc.h, src/render/screen.cpp

## Building

See README.md for build instructions. The project uses:
- devkitPro devkitPPC toolchain
- Docker for reproducible builds
- WUPS backend libraries

## Resources

### Official Documentation
- [WUT Documentation](https://wut.devkitpro.org/) - Wii U system API reference
- [WUPS Wiki](https://github.com/wiiu-env/WiiUPluginSystem/wiki) - Plugin development guide

### Community Resources
- [GBAtemp Wii U Hacking](https://gbatemp.net/forums/wii-u-hacking-backup-loaders.247/) - Community forum
- [Wii U Brew Wiki](https://wiiubrew.org/) - Technical documentation

### Source Code References
- [WiiUPluginLoaderBackend](https://github.com/wiiu-env/WiiUPluginLoaderBackend) - WUPS loader, DrawUtils
- [NotificationModule](https://github.com/wiiu-env/NotificationModule) - Notification system
- [libmappedmemory](https://github.com/wiiu-env/libmappedmemory) - GX2-compatible memory allocation
