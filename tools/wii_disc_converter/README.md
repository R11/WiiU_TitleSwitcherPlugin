# Wii U Virtual Console Injector

Linux scripts for converting ROMs and disc images to Wii U installable WUP format, without requiring Windows.

## Scripts

| Script | Purpose | Systems |
|--------|---------|---------|
| `inject.sh` | Inject ROMs into VC format (native Linux) | NES, SNES, N64, GBA, DS, TG16 |
| `convert.sh` | Convert disc images via Wine | Wii, GameCube |

## Quick Start

```bash
# 1. Install dependencies
./inject.sh --setup

# 2. Add your Wii U common key
echo "YOUR_32_CHARACTER_HEX_KEY" > common_key.txt

# 3. Set up a base game (dump from your Wii U)
./inject.sh --setup-base /path/to/vc/dump bases/gba_base

# 4. Inject a ROM
./inject.sh -s gba -r "Metroid Fusion.gba" -b bases/gba_base
```

## Supported Systems

### Native Linux (inject.sh)

| System | Extensions | ROM Location in VC |
|--------|------------|-------------------|
| NES | `.nes`, `.fds` | Embedded in RPX |
| SNES | `.sfc`, `.smc` | Embedded in RPX |
| N64 | `.z64`, `.n64`, `.v64` | `content/rom/` |
| GBA | `.gba`, `.gb`, `.gbc` | `content/alldata.psb.m` |
| DS | `.nds`, `.srl` | `content/0010/rom.nds` |
| TG16 | `.pce` | Embedded in RPX |

### Via Wine (convert.sh)

| System | Extensions |
|--------|------------|
| Wii | `.iso`, `.wbfs`, `.wia`, `.rvz`, `.ciso` |
| GameCube | `.iso`, `.gcm`, `.gcz`, `.ciso`, `.nkit.iso` |

## Requirements

- **Java 8+** - for NUSPacker
- **Python 3** - for inject_gba (GBA injection)
- **ImageMagick** - for cover art conversion
- **Wii U Common Key** - for creating installable packages
- **Base VC Games** - decrypted dumps from your Wii U

For Wii/GameCube (convert.sh):
- **wit** (Wiimms ISO Tools)
- **Wine** + .NET Framework 4.6.1

## inject.sh Usage

### Single ROM

```bash
./inject.sh --system gba --rom game.gba --base bases/gba_base

# With custom title and cover
./inject.sh -s snes -r game.sfc -b bases/snes_base \
    --title "My Game" --cover cover.png
```

### Batch Mode

```bash
# Inject all GBA ROMs in a folder
./inject.sh --batch -s gba --roms ./gba_roms -b bases/gba_base
```

### Options

| Option | Description |
|--------|-------------|
| `--system`, `-s` | Target system (nes, snes, n64, gba, ds, tg16) |
| `--rom`, `-r` | ROM file to inject |
| `--base`, `-b` | Decrypted base VC game folder |
| `--output`, `-o` | Output directory (default: ./output) |
| `--title`, `-t` | Custom game title |
| `--cover` | Custom cover image (PNG/JPG) |
| `--no-artwork` | Skip GameTDB cover download |
| `--batch` | Process multiple ROMs |
| `--roms` | Folder containing ROMs (batch mode) |
| `--dry-run` | Preview without making changes |

## Setting Up Base Games

You need a decrypted base VC game for each system. These come from your own Wii U:

1. **Dump a VC game** using [dumpling](https://github.com/emiyl/dumpling)
2. **Decrypt it** (if encrypted):
   ```bash
   ./inject.sh --setup-base /path/to/dump bases/gba_base
   ```
3. The base folder should contain `code/`, `content/`, and `meta/` directories

### Recommended Base Games

| System | Suggested Base |
|--------|---------------|
| NES | Any NES VC title |
| SNES | Any SNES VC title |
| N64 | Donkey Kong 64, Paper Mario |
| GBA | Minish Cap, Metroid Fusion |
| DS | Brain Age, WarioWare |
| TG16 | Any TG16 VC title |

## Features

### Automatic Cover Art

The script downloads cover art from [GameTDB](https://www.gametdb.com/) automatically:

- Icons (128x128)
- TV boot screen (1280x720)
- GamePad boot screen (854x480)

Covers are cached in `.cache/covers/` to avoid re-downloading.

### Title ID Generation

Unique title IDs are generated based on the game ID and system, ensuring:
- No conflicts with retail titles
- Reproducible IDs for the same game
- Custom IDs can be specified with `--title-id`

### Metadata

The script generates proper `meta.xml` with:
- Game title in all languages
- Publisher info
- Title ID and version

## convert.sh Usage (Wii/GameCube)

For Wii and GameCube games, use `convert.sh` which uses Wine to run TeconMoon's WiiVC Injector:

```bash
# Setup Wine (first time only, takes 10-15 mins)
./convert.sh --setup
./convert.sh --setup-wine

# Convert games
./convert.sh /path/to/roms /path/to/output
./convert.sh --single "Super Mario Sunshine.iso"
```

See the comments in `convert.sh` for more details.

## Output

Both scripts produce WUP packages for installation:

1. Copy output folders to `SD:/install/`
2. Boot Wii U with CFW (Tiramisu/Aroma)
3. Run **WUP Installer GX2**
4. Install the packages

## How Injection Works

### GBA

The GBA VC uses `alldata.psb.m` to store the ROM in a compressed PSB format. The `inject_gba` tool handles extraction and re-injection.

### DS

DS ROMs are stored directly as `rom.nds` (or `rom.zip`) in the `content/0010/` folder. Simple file replacement.

### N64

N64 ROMs go in `content/rom/` with a matching INI config in `content/config/`. Compatibility depends on having the right INI settings.

### NES/SNES/TG16

ROMs are embedded in the RPX executable. Requires `wiiurpxtool` to decompress, find ROM offset, inject, and recompress.

### Wii/GameCube

Creates a Wii disc image containing the game plus a Nintendont autoboot forwarder, wrapped as a Wii VC channel.

## Troubleshooting

### "inject_gba not found"

```bash
./inject.sh --setup
# Or manually:
pip3 install ./deps/inject_gba --user
```

### Cover art not downloading

- Check your internet connection
- Game ID may not be in GameTDB database
- Use `--cover` to provide custom artwork
- Use `--no-artwork` to skip

### Games don't work

- Ensure base game is fully decrypted
- Check that signature patches are active (Tiramisu/Aroma)
- For N64, you may need a custom INI config
- Some games have compatibility issues with VC emulators

### Common key errors

Your `common_key.txt` must:
- Contain exactly 32 hex characters
- Have no spaces or newlines
- Be in the script directory

## Credits

- **ajd4096** - inject_gba
- **0CBH0** - wiiurpxtool
- **VitaSmith** - cdecrypt
- **ihaveamac** - NUSPacker
- **TeconMoon** - Original WiiVC Injector
- **FIX94** - Nintendont
- **GameTDB** - Cover art database

## Links

- [inject_gba](https://github.com/ajd4096/inject_gba)
- [wiiurpxtool](https://github.com/0CBH0/wiiurpxtool)
- [cdecrypt](https://github.com/VitaSmith/cdecrypt)
- [NUSPacker](https://github.com/ihaveamac/nuspacker)
- [GameTDB](https://www.gametdb.com/)
- [UWUVCI Guides](https://uwuvci-prime.github.io/UWUVCI-Resources/)
