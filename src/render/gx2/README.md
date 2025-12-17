# GX2 Rendering Backend

This directory contains the GX2-based rendering backend for colored text and advanced graphics.

## Architecture

Based on the [NotificationModule](https://github.com/wiiu-env/NotificationModule) implementation:

### Components

1. **schrift.c/h** - Lightweight TrueType font rasterizer (ISC licensed from [libschrift](https://github.com/tomolt/libschrift))
2. **SchriftGX2.cpp/h** - GX2 wrapper that converts glyphs to textures
3. **shaders/** - Pre-compiled GX2 shaders
   - Texture2DShader - For rendering textured quads with color
   - ColorShader - For solid color rendering
4. **gx2_overlay.cpp/h** - Hooks into game rendering to draw overlay

### How It Works

1. Hook `GX2CopyColorBufferToScanBuffer` via libfunctionpatcher
2. Before the game's frame is displayed, draw our UI into the color buffer
3. Save/restore GX2 context state to not disrupt game rendering

### Dependencies

- libfunctionpatcher - For hooking GX2 functions
- libmappedmemory - For GX2-compatible memory allocation
- wut GX2 headers - For GX2 API access

### References

- [NotificationModule](https://github.com/wiiu-env/NotificationModule) - Reference implementation
- [libschrift](https://github.com/tomolt/libschrift) - Font rasterizer
- [wut GX2 docs](https://wut.devkitpro.org/group__gx2.html) - GX2 API documentation
