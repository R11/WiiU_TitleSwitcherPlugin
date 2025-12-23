# Wii U VC Injector - Development Notes

This document explains how the injection tools work and how to continue development.

## Architecture Overview

```
inject.sh                 # Main entry point (bash)
├── rpx_inject.py        # SNES/NES ROM injection (Python)
├── deps/
│   ├── wiiurpxtool      # RPX compress/decompress
│   ├── inject_gba/      # GBA ROM injection (Python)
│   ├── cdecrypt         # Wii U content decryption
│   └── NUSPacker.jar    # WUP packaging (Java)
└── convert.sh           # Wii/GameCube (uses Wine)
```

## How Each System Works

### GBA (✅ Complete)

**Location:** `content/alldata.psb.m`

The GBA VC uses M2's emulator which stores the ROM in a PSB container format.

```bash
# The inject_gba tool handles this:
inject_gba --inpsb alldata.psb.m --inrom game.gba --outpsb new.psb.m
```

**Key files:**
- `content/alldata.psb.m` - Contains the ROM in PSB format
- `content/alldata.bin` - Associated data file
- `code/m2engage.rpx` - M2's GBA emulator

### DS (✅ Complete)

**Location:** `content/0010/rom.nds` or `content/0010/rom.zip`

DS injection is simple file replacement. The emulator loads the ROM from a known path.

```bash
# Just copy the ROM:
cp game.nds content/0010/rom.nds
# Or if base uses zip:
zip content/0010/rom.zip game.nds
```

**Notes:**
- ROM must be ≤ base game ROM size
- Some bases use `rom.zip` (older), some use `rom.nds` (newer)
- DSi-enhanced games don't work

### SNES (✅ Implemented, needs testing)

**Location:** Embedded in `code/*.rpx`

The ROM is embedded directly in the RPX executable. The injection process:

1. **Decompress RPX** using `wiiurpxtool -d`
2. **Find ROM offset** - search for "WUP-" header, ROM follows
3. **Replace ROM data** - must be ≤ original size
4. **Update checksums** - SNES internal header checksum
5. **Recompress RPX** using `wiiurpxtool -c`

**WUP Header structure:**
```
Offset  Size  Description
0x00    4     "WUP-" signature
0x04    4     Title ID (e.g., "JACE" for Super Metroid)
0x0E    1     ROM size: 0x40=4MB, 0x20=2MB, 0x10=1MB, etc.
0x01    1     System: 0x01=SNES, 0x00=NES
~0x20+  -     ROM data starts (varies by game)
```

**ROM offset detection:**
- Search for "WUP-" signature
- ROM typically starts 0x20-0x100 bytes after
- Verify by checking for valid SNES header at offset + 0x7FC0 (LoROM) or + 0xFFC0 (HiROM)

**Known offsets** (from community):
- Many games: `0x788480`
- Varies significantly - auto-detection recommended

### NES (✅ Implemented, needs testing)

Same process as SNES but:
- System byte at WUP+0x01 is 0x00
- No internal checksum to update
- ROM header (iNES) at offset 0x00

### N64 (⚠️ Partial)

**Location:** `content/rom/` directory

N64 ROMs are stored as files, not embedded:

```
content/
├── rom/
│   └── GAMEID.z64    # The ROM file (name varies)
└── config/
    └── GAMEID.ini    # Emulator configuration (required!)
```

**Key challenge:** N64 games need matching INI config files for the emulator. Without the right INI, games may not boot or have issues.

**INI database:** https://wiki.gbatemp.net/wiki/WiiU_VC_N64_inject_compatibility_list

**TODO:**
- [ ] Build INI config database
- [ ] Auto-match games to configs by title/ID
- [ ] Handle ROM format conversion (v64/n64 → z64)

### TG16 / PC Engine (❌ Not implemented)

Similar to NES/SNES - ROM embedded in RPX. Needs research on:
- WUP header location
- ROM offset patterns
- Any system-specific requirements

### Wii / GameCube (✅ Via Wine)

These use a completely different approach - they create a Wii disc image containing:
- Nintendont autoboot forwarder (DOL)
- The GameCube ISO

This gets wrapped as a Wii VC channel. Still requires Wine for TeconMoon's injector.

**Native approach (TODO):**
- Use `wit` to compose Wii disc from FST
- Download pre-compiled forwarder DOLs from FIX94
- Would eliminate Wine dependency

## Testing Checklist

### To test SNES injection:

```bash
# 1. Set up dependencies
./inject.sh --setup

# 2. Create common_key.txt with your Wii U common key
echo "YOUR_32_HEX_CHARS" > common_key.txt

# 3. Dump an SNES VC game from your Wii U using dumpling
# Put it in bases/snes_base/

# 4. Decrypt if needed
./inject.sh --setup-base /path/to/dump bases/snes_base

# 5. Test injection
./inject.sh -s snes -r "Super Mario World.sfc" -b bases/snes_base --dry-run

# 6. Actually inject
./inject.sh -s snes -r "Super Mario World.sfc" -b bases/snes_base

# 7. Install on Wii U and test!
```

### If auto-detection fails:

```bash
# 1. Decompress RPX manually
./deps/wiiurpxtool -d bases/snes_base/code/game.rpx

# 2. Open in hex editor, search for "WUP-"
# Note the offset where ROM data starts

# 3. Use manual offset
python3 rpx_inject.py bases/snes_base/code/game.rpx game.sfc snes --offset 0x788480
```

## Key Resources

### Tools
- **wiiurpxtool**: https://github.com/0CBH0/wiiurpxtool
- **inject_gba**: https://github.com/ajd4096/inject_gba
- **cdecrypt**: https://github.com/VitaSmith/cdecrypt
- **NUSPacker**: https://github.com/ihaveamac/nuspacker
- **wit**: https://wit.wiimm.de/

### Documentation
- **UWUVCI Guides**: https://uwuvci-prime.github.io/UWUVCI-Resources/
- **Wii U VC Reversing**: https://www.retroreversing.com/WiiUVirtualConsole
- **N64 Compatibility**: https://wiki.gbatemp.net/wiki/WiiU_VC_N64_inject_compatibility_list
- **SNES RPX Settings**: https://gbatemp.net/threads/understanding-and-changing-snes-vc-rpx-settings.425474/

### Related Projects
- **VinjeCtiine**: https://github.com/wmltogether/VinjeCtiine (Python SNES/NES injector)
- **Phacox's Injector**: https://github.com/phacoxcll/PhacoxsInjector (C# all-in-one)
- **UWUVCI-AIO**: https://github.com/stuff-by-3-random-dudes/UWUVCI-AIO-WPF (C# Windows)

## File Formats

### meta.xml

```xml
<?xml version="1.0" encoding="utf-8"?>
<menu>
  <title_id type="hexBinary" length="8">0005000010XXXXXX</title_id>
  <longname_en>Game Title</longname_en>
  <shortname_en>Game Title</shortname_en>
  <publisher_en>Publisher</publisher_en>
  <!-- ... other languages ... -->
</menu>
```

### app.xml

```xml
<?xml version="1.0" encoding="utf-8"?>
<app type="complex" access="777">
  <title_id type="hexBinary" length="8">0005000010XXXXXX</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <app_type type="hexBinary" length="4">80000000</app_type>
</app>
```

### TGA Images

| File | Size | Format |
|------|------|--------|
| iconTex.tga | 128x128 | 32-bit RGBA |
| bootTvTex.tga | 1280x720 | 24-bit RGB |
| bootDrcTex.tga | 854x480 | 24-bit RGB |
| bootLogoTex.tga | 170x42 | 32-bit RGBA |

No RLE compression. Use ImageMagick:
```bash
convert input.png -resize 128x128! -depth 8 TGA:iconTex.tga
```

## Next Steps / TODO

### High Priority
- [ ] Test SNES injection with real hardware
- [ ] Test NES injection
- [ ] Build ROM offset database for common base games

### Medium Priority
- [ ] Implement TG16 injection
- [ ] Add N64 INI config matching
- [ ] Native Wii/GC injection without Wine

### Nice to Have
- [ ] GUI wrapper (Python tkinter or web-based)
- [ ] Batch injection progress bar
- [ ] ROM database integration (No-Intro, TOSEC)

## Troubleshooting

### "Could not auto-detect ROM offset"
- Open decompressed RPX in hex editor
- Search for "WUP-" string
- ROM typically starts 0x20-0x100 bytes after
- Use `--offset` parameter

### "ROM is larger than base game"
- Use a different base game with larger ROM
- Or use a trimmed/patched version of your ROM

### Games boot but crash/glitch
- Base game may have game-specific emulator patches
- Try different base game
- Check SNES ROM type (LoROM vs HiROM) matches base

### Cover art not downloading
- Check internet connection
- Game ID may not be in GameTDB
- Provide custom cover with `--cover`

## Contributing

If you improve this tool:
1. Test on real hardware
2. Document any new ROM offsets found
3. Add notes to this file
4. Share back to the community!
