# GX2 Rendering Backend

This directory contains the GX2-based rendering backend for colored text and advanced graphics.

## Status

**Work in Progress** - The framework is in place but not yet functional.

### What's Implemented

- **schrift.c/h** - Font rasterizer, modified for Wii U (no file I/O)
- **SchriftGX2.cpp/h** - Glyph-to-texture converter (needs embedded font data)
- **ColorShader** - Solid rectangle rendering (needs compiled shader binaries)
- **Texture2DShader** - Textured quad rendering (needs compiled shader binaries)
- **gx2_overlay.cpp/h** - GX2 function hooks and overlay drawing

### What's Missing

1. **Compiled shader binaries** - Need GPU7 compiler tooling to create actual .gsh files
2. **Embedded font data** - Need to embed a TrueType font (e.g., DejaVu Sans)
3. **Testing** - Needs to be tested on actual hardware

### Enabling GX2 Rendering

Currently disabled via `#ifdef ENABLE_GX2_RENDERING`. To enable:

1. Compile shader source code to GPU7 binaries
2. Embed font data
3. Add `-DENABLE_GX2_RENDERING` to CFLAGS in Makefile
4. Add `-lfunctionpatcher` to LIBS in Makefile
5. Test on hardware

## Architecture

Based on the [NotificationModule](https://github.com/wiiu-env/NotificationModule) implementation:

### Components

1. **schrift.c/h** - Lightweight TrueType font rasterizer (ISC licensed from [libschrift](https://github.com/tomolt/libschrift))
2. **SchriftGX2.cpp/h** - GX2 wrapper that converts glyphs to textures
3. **shaders/** - GX2 shaders (placeholder implementations)
   - Texture2DShader - For rendering textured quads with color tint
   - ColorShader - For solid color rectangle rendering
4. **gx2_overlay.cpp/h** - Hooks into game rendering to draw overlay

### How It Works

1. Hook `GX2SetContextState` to capture game's context state
2. Hook `GX2CopyColorBufferToScanBuffer` to intercept frame display
3. Before the game's frame is displayed, draw our UI into the color buffer
4. Restore game's GX2 context state to not disrupt rendering

### Dependencies

- libfunctionpatcher - For hooking GX2 functions (via WUPS)
- libmappedmemory - For GX2-compatible memory allocation
- wut GX2 headers - For GX2 API access

### References

- [NotificationModule](https://github.com/wiiu-env/NotificationModule) - Reference implementation
- [libschrift](https://github.com/tomolt/libschrift) - Font rasterizer
- [wut GX2 docs](https://wut.devkitpro.org/group__gx2.html) - GX2 API documentation
