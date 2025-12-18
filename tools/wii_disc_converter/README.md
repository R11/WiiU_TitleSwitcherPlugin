# Wii/GameCube Disc to Wii U Converter

A Linux script that converts GameCube and Wii disc images to Wii U installable WUP format, similar to what TeconMoon's WiiVC Injector does on Windows.

## Overview

This tool helps you:
1. **Convert disc images** from various formats (WBFS, NKit, CISO, etc.) to standard ISO
2. **Inject games** into Wii Virtual Console format using TeconMoon's Injector via Wine
3. **Package as WUP** for installation via WUP Installer GX2

## Requirements

- **Linux** (Ubuntu, Debian, Arch, Fedora, or similar)
- **wit** (Wiimms ISO Tools) - for disc format conversion
- **Wine** + .NET Framework 4.6.1 - for running TeconMoon's WiiVC Injector
- **Java 8+** - for NUSPacker
- **Wii U Common Key** - for creating installable packages

## Quick Start

```bash
# 1. Make the script executable
chmod +x convert.sh

# 2. Install dependencies
./convert.sh --setup

# 3. Set up Wine and the injector (takes 10-15 mins first time)
./convert.sh --setup-wine

# 4. Add your Wii U common key
echo "YOUR_32_CHARACTER_HEX_KEY" > common_key.txt

# 5. Convert your games!
./convert.sh /path/to/your/roms /path/to/output
```

## Detailed Setup

### Step 1: Basic Dependencies

```bash
./convert.sh --setup
```

This installs:
- **wit** - Wiimms ISO Tools for disc image conversion
- **Java** - Required for NUSPacker
- **NUSPacker** - Creates installable WUP packages
- **Nintendont** - Required for GameCube games

### Step 2: Wine Setup (for full functionality)

```bash
./convert.sh --setup-wine
```

This installs:
- **Wine** - Windows compatibility layer
- **TeconMoon's WiiVC Injector** - The injection tool
- **.NET Framework 4.6.1** - Required by the injector

The .NET installation can take 10-15 minutes on first run.

### Step 3: Common Key

You need the Wii U common key to create installable packages. This is a 32-character hexadecimal string. Create a file called `common_key.txt` in the script directory with your key:

```bash
echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" > common_key.txt
```

The key is required by NUSPacker to encrypt the packages properly.

## Usage

### Convert a folder of games

```bash
./convert.sh /path/to/roms /path/to/output
```

### Convert a single game

```bash
./convert.sh --single "Super Mario Sunshine.iso" ./output
```

### GameCube only

```bash
./convert.sh --gc-only /path/to/roms
```

### Wii only

```bash
./convert.sh --wii-only /path/to/roms
```

### Dry run (see what would happen)

```bash
./convert.sh --dry-run /path/to/roms
```

## Supported Input Formats

| Format | Extension | Notes |
|--------|-----------|-------|
| ISO | `.iso` | Standard disc image |
| GCM | `.gcm` | GameCube disc image |
| WBFS | `.wbfs` | Wii Backup File System |
| NKit | `.nkit.iso` | Compressed/trimmed format |
| CISO | `.ciso` | Compact ISO |
| WIA | `.wia` | Wii ISO Archive |
| RVZ | `.rvz` | Modern compressed format |
| GCZ | `.gcz` | Dolphin's compressed format |

## Output

The tool produces WUP packages that can be installed on a homebrew-enabled Wii U:

1. Copy the output folders to `SD:/install/`
2. Boot your Wii U with CFW (Tiramisu/Aroma)
3. Run **WUP Installer GX2**
4. Install the packages

### For GameCube Games

GameCube games require Nintendont to be installed:
- Place `boot.dol` at `SD:/apps/nintendont/boot.dol`
- The converter downloads this automatically during setup

## How It Works

### For Wii Games

1. Converts the disc image to standard ISO format using `wit`
2. Launches TeconMoon's WiiVC Injector via Wine
3. The injector wraps the game as a Wii VC channel
4. NUSPacker creates the installable WUP package

### For GameCube Games

1. Converts the disc image to standard ISO format
2. TeconMoon's Injector creates a special Wii disc containing:
   - The GameCube ISO
   - A Nintendont autoboot forwarder
3. This is then wrapped as a Wii VC channel
4. NUSPacker creates the WUP package

When launched on Wii U, the game:
1. Boots into vWii mode
2. Nintendont loads automatically
3. The GameCube game starts

## Troubleshooting

### Wine crashes or .NET won't install

Try using a fresh Wine prefix:
```bash
rm -rf deps/wine_prefix
./convert.sh --setup-wine
```

### "wit not found"

Install manually:
```bash
# Ubuntu/Debian
sudo apt install wit

# Arch
sudo pacman -S wiimms-iso-tools
```

### Common key errors

Make sure your `common_key.txt`:
- Contains exactly 32 hexadecimal characters
- Has no spaces or newlines
- Is in the same directory as `convert.sh`

### Games don't work after installation

- Ensure you have signature patches (Tiramisu/Aroma have this by default)
- For GameCube, ensure Nintendont is properly installed
- Try a different base game in the injector

## Alternative: Manual Wine Approach

If the automated Wine setup doesn't work, you can:

1. Install Wine manually: `sudo apt install wine`
2. Download TeconMoon's WiiVC Injector from [GitHub](https://github.com/piratesephiroth/TeconmoonWiiVCInjector/releases)
3. Run with: `wine "TeconMoon's WiiVC Injector.exe"`

## Credits

- **TeconMoon** - Original WiiVC Injector
- **piratesephiroth** - WiiVC Injector fork with NKit support
- **FIX94** - Nintendont
- **Wiimm** - Wiimms ISO Tools
- **ihaveamac** - NUSPacker fork

## Links

- [TeconMoon's WiiVC Injector](https://github.com/piratesephiroth/TeconmoonWiiVCInjector)
- [Wiimms ISO Tools](https://wit.wiimm.de/)
- [NUSPacker](https://github.com/ihaveamac/nuspacker)
- [Nintendont](https://github.com/FIX94/Nintendont)
- [UWUVCI Guide](https://uwuvci-prime.github.io/UWUVCI-Resources/)
