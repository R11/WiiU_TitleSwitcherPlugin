#!/bin/bash
#
# Wii U Virtual Console Injector
#
# Converts ROMs from various consoles to Wii U installable WUP format.
# Supports: NES, SNES, N64, GBA, DS, TG16, Wii, GameCube
#
# Usage:
#   ./inject.sh --system <system> --rom <rom_file> --base <base_folder> [options]
#   ./inject.sh --setup                    # Install dependencies
#   ./inject.sh --help                     # Show help
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/deps"
BASES_DIR="$SCRIPT_DIR/bases"
CACHE_DIR="$SCRIPT_DIR/.cache"
TEMP_DIR="$SCRIPT_DIR/.tmp"
OUTPUT_DIR="$SCRIPT_DIR/output"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration files
COMMON_KEY_FILE="$SCRIPT_DIR/common_key.txt"
NINCFG_TEMPLATE="$SCRIPT_DIR/nincfg_template.bin"

# Supported systems
SUPPORTED_SYSTEMS="nes snes n64 gba ds tg16 wii gc"

# ROM file extensions by system
declare -A ROM_EXTENSIONS=(
    [nes]="nes fds"
    [snes]="sfc smc"
    [n64]="z64 n64 v64"
    [gba]="gba gb gbc"
    [ds]="nds srl"
    [tg16]="pce"
    [wii]="iso wbfs wia rvz ciso"
    [gc]="iso gcm gcz ciso"
)

# GameTDB system codes
declare -A GAMETDB_SYSTEMS=(
    [nes]="NES"
    [snes]="SNES"
    [n64]="N64"
    [gba]="GBA"
    [ds]="DS"
    [tg16]="PCE"
    [wii]="Wii"
    [gc]="GC"
)

#------------------------------------------------------------------------------
# Utility Functions
#------------------------------------------------------------------------------

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${CYAN}[STEP]${NC} $1"
}

die() {
    log_error "$1"
    exit 1
}

check_command() {
    command -v "$1" &> /dev/null
}

show_help() {
    cat << 'EOF'
Wii U Virtual Console Injector
================================

Injects ROMs into Wii U Virtual Console format for installation.

USAGE:
    ./inject.sh --system <sys> --rom <file> --base <folder> [options]
    ./inject.sh --batch --system <sys> --roms <folder> --base <folder>
    ./inject.sh --setup
    ./inject.sh --help

REQUIRED ARGUMENTS:
    --system, -s     Target system: nes, snes, n64, gba, ds, tg16, wii, gc
    --rom, -r        ROM file to inject
    --base, -b       Decrypted base VC game folder (must have code/content/meta)

OPTIONS:
    --output, -o     Output directory (default: ./output)
    --title, -t      Custom title for the game
    --title-id       Custom title ID (16 hex chars, default: auto-generated)
    --cover          Custom cover image (PNG/JPG, will be converted to TGA)
    --no-artwork     Skip downloading artwork from GameTDB
    --batch          Process multiple ROMs from a folder
    --roms           Folder containing ROMs (for batch mode)
    --dry-run        Show what would be done without doing it
    --keep-temp      Don't delete temporary files

SETUP OPTIONS:
    --setup          Install all dependencies
    --setup-base     Extract/decrypt a base game from WUP files

SUPPORTED SYSTEMS:
    nes     Nintendo Entertainment System (.nes, .fds)
    snes    Super Nintendo (.sfc, .smc)
    n64     Nintendo 64 (.z64, .n64, .v64)
    gba     Game Boy Advance (.gba, .gb, .gbc)
    ds      Nintendo DS (.nds, .srl)
    tg16    TurboGrafx-16 / PC Engine (.pce)
    wii     Nintendo Wii (.iso, .wbfs, .wia, .rvz)
    gc      Nintendo GameCube (.iso, .gcm, .gcz)

EXAMPLES:
    # Set up dependencies
    ./inject.sh --setup

    # Decrypt a base game for injection
    ./inject.sh --setup-base /path/to/wup/files /path/to/output

    # Inject a single GBA ROM
    ./inject.sh -s gba -r "Metroid Fusion.gba" -b bases/gba_base

    # Inject with custom title and cover
    ./inject.sh -s snes -r game.sfc -b bases/snes_base \
        --title "My Game" --cover cover.png

    # Batch inject all NES ROMs in a folder
    ./inject.sh --batch -s nes --roms ./nes_roms -b bases/nes_base

REQUIREMENTS:
    - Java 8+ (for NUSPacker)
    - Python 3 (for inject_gba)
    - ImageMagick (for cover art conversion)
    - Wii U common key in common_key.txt
    - Decrypted base VC games for each system you want to inject

BASE GAME SETUP:
    You need a decrypted base VC game for each system. To set up:
    1. Dump a VC game from your Wii U using dumpling
    2. Run: ./inject.sh --setup-base /path/to/dump bases/gba_base
    3. Use that base folder for injections

EOF
}

#------------------------------------------------------------------------------
# Dependency Management
#------------------------------------------------------------------------------

install_imagemagick() {
    log_info "Installing ImageMagick..."
    if check_command apt-get; then
        sudo apt-get update && sudo apt-get install -y imagemagick
    elif check_command pacman; then
        sudo pacman -S --noconfirm imagemagick
    elif check_command dnf; then
        sudo dnf install -y ImageMagick
    elif check_command brew; then
        brew install imagemagick
    else
        die "Could not install ImageMagick. Please install manually."
    fi
    log_success "ImageMagick installed"
}

install_python3() {
    log_info "Installing Python 3..."
    if check_command apt-get; then
        sudo apt-get update && sudo apt-get install -y python3 python3-pip
    elif check_command pacman; then
        sudo pacman -S --noconfirm python python-pip
    elif check_command dnf; then
        sudo dnf install -y python3 python3-pip
    elif check_command brew; then
        brew install python3
    else
        die "Could not install Python 3. Please install manually."
    fi
    log_success "Python 3 installed"
}

install_java() {
    log_info "Installing Java..."
    if check_command apt-get; then
        sudo apt-get update && sudo apt-get install -y default-jre
    elif check_command pacman; then
        sudo pacman -S --noconfirm jre-openjdk
    elif check_command dnf; then
        sudo dnf install -y java-latest-openjdk
    elif check_command brew; then
        brew install openjdk
    else
        die "Could not install Java. Please install Java 8+ manually."
    fi
    log_success "Java installed"
}

download_cdecrypt() {
    log_info "Downloading cdecrypt..."
    mkdir -p "$TOOLS_DIR"

    local CDECRYPT_URL="https://github.com/VitaSmith/cdecrypt/releases/latest/download/cdecrypt"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        CDECRYPT_URL="https://github.com/VitaSmith/cdecrypt/releases/latest/download/cdecrypt_macos"
    fi

    curl -L "$CDECRYPT_URL" -o "$TOOLS_DIR/cdecrypt"
    chmod +x "$TOOLS_DIR/cdecrypt"

    log_success "cdecrypt downloaded to $TOOLS_DIR/cdecrypt"
}

download_nuspacker() {
    log_info "Downloading NUSPacker..."
    mkdir -p "$TOOLS_DIR"

    local NUSPACKER_URL="https://github.com/ihaveamac/nuspacker/releases/download/v1.2/NUSPacker.jar"
    curl -L "$NUSPACKER_URL" -o "$TOOLS_DIR/NUSPacker.jar"

    log_success "NUSPacker downloaded"
}

download_inject_gba() {
    log_info "Downloading inject_gba..."
    mkdir -p "$TOOLS_DIR"

    local INJECT_GBA_URL="https://github.com/ajd4096/inject_gba/archive/refs/heads/master.zip"
    curl -L "$INJECT_GBA_URL" -o "$TOOLS_DIR/inject_gba.zip"

    cd "$TOOLS_DIR"
    unzip -o inject_gba.zip
    mv inject_gba-master inject_gba
    cd inject_gba

    if check_command pip3; then
        pip3 install . --user
    elif check_command pip; then
        pip install . --user
    fi

    cd "$SCRIPT_DIR"
    log_success "inject_gba downloaded and installed"
}

download_wiiurpxtool() {
    log_info "Downloading wiiurpxtool..."
    mkdir -p "$TOOLS_DIR"

    local RPXTOOL_URL="https://github.com/0CBH0/wiiurpxtool/releases/latest/download/wiiurpxtool"
    curl -L "$RPXTOOL_URL" -o "$TOOLS_DIR/wiiurpxtool"
    chmod +x "$TOOLS_DIR/wiiurpxtool"

    log_success "wiiurpxtool downloaded"
}

create_nincfg_template() {
    log_info "Creating default Nintendont config template..."

    # Create a default nincfg.bin with good settings:
    # - Memory card emulation enabled
    # - Wii U Widescreen enabled
    # - Native control enabled
    # This is a binary file, so we create it with printf

    # NIN_CFG structure (simplified):
    # Magic: 0x01070CF6 (4 bytes)
    # Version: 9 (4 bytes)
    # Config flags (4 bytes)
    # VideoMode (4 bytes)
    # Language (4 bytes)
    # GamePath (256 bytes)
    # ... more fields

    # For now, we'll note that users should generate this via Nintendont itself
    log_warn "For GameCube games, configure Nintendont settings on your Wii U first"
    log_warn "The nincfg.bin will be read from SD:/nincfg.bin"
}

do_setup() {
    log_info "Setting up Wii U Virtual Console Injector..."
    echo ""

    # Check and install dependencies
    if ! check_command java; then
        install_java
    else
        log_success "Java is already installed"
    fi

    if ! check_command python3; then
        install_python3
    else
        log_success "Python 3 is already installed"
    fi

    if ! check_command convert; then
        install_imagemagick
    else
        log_success "ImageMagick is already installed"
    fi

    # Download tools
    if [[ ! -f "$TOOLS_DIR/cdecrypt" ]]; then
        download_cdecrypt
    else
        log_success "cdecrypt already present"
    fi

    if [[ ! -f "$TOOLS_DIR/NUSPacker.jar" ]]; then
        download_nuspacker
    else
        log_success "NUSPacker already present"
    fi

    if [[ ! -d "$TOOLS_DIR/inject_gba" ]]; then
        download_inject_gba
    else
        log_success "inject_gba already present"
    fi

    if [[ ! -f "$TOOLS_DIR/wiiurpxtool" ]]; then
        download_wiiurpxtool
    else
        log_success "wiiurpxtool already present"
    fi

    # Create directories
    mkdir -p "$BASES_DIR"
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$CACHE_DIR/covers"

    echo ""
    log_success "Setup complete!"
    echo ""
    log_info "Next steps:"
    log_info "  1. Place your Wii U common key in: $COMMON_KEY_FILE"
    log_info "     (32 hex characters, no spaces)"
    log_info "  2. Set up base games for each system you want to use:"
    log_info "     ./inject.sh --setup-base /path/to/vc/dump bases/gba_base"
    log_info "  3. Start injecting!"
}

#------------------------------------------------------------------------------
# Base Game Setup
#------------------------------------------------------------------------------

setup_base_game() {
    local input_dir="$1"
    local output_dir="$2"

    if [[ -z "$input_dir" ]] || [[ -z "$output_dir" ]]; then
        die "Usage: --setup-base <input_wup_folder> <output_base_folder>"
    fi

    log_info "Setting up base game from: $input_dir"

    # Check if it's already decrypted (has code/content/meta folders)
    if [[ -d "$input_dir/code" ]] && [[ -d "$input_dir/content" ]] && [[ -d "$input_dir/meta" ]]; then
        log_info "Input appears to be already decrypted, copying..."
        mkdir -p "$output_dir"
        cp -r "$input_dir/code" "$output_dir/"
        cp -r "$input_dir/content" "$output_dir/"
        cp -r "$input_dir/meta" "$output_dir/"
        log_success "Base game copied to: $output_dir"
        return 0
    fi

    # Check for encrypted NUS content
    if [[ ! -f "$input_dir/title.tmd" ]] && [[ ! -f "$input_dir/tmd" ]]; then
        die "Input doesn't appear to be a valid WUP dump or decrypted game"
    fi

    # Decrypt using cdecrypt
    if [[ ! -f "$TOOLS_DIR/cdecrypt" ]]; then
        die "cdecrypt not found. Run --setup first."
    fi

    if [[ ! -f "$COMMON_KEY_FILE" ]]; then
        die "Common key not found at: $COMMON_KEY_FILE"
    fi

    log_info "Decrypting NUS content..."
    mkdir -p "$output_dir"

    # Create keys.txt for cdecrypt
    local common_key
    common_key=$(cat "$COMMON_KEY_FILE" | tr -d '[:space:]')
    echo "$common_key" > "$output_dir/keys.txt"

    "$TOOLS_DIR/cdecrypt" "$input_dir" "$output_dir"

    rm -f "$output_dir/keys.txt"

    log_success "Base game decrypted to: $output_dir"
}

#------------------------------------------------------------------------------
# Cover Art / Metadata
#------------------------------------------------------------------------------

get_game_id_from_rom() {
    local rom_file="$1"
    local system="$2"

    case "$system" in
        gba)
            # GBA game ID is at offset 0xAC, 4 bytes
            dd if="$rom_file" bs=1 skip=172 count=4 2>/dev/null | tr -d '\0'
            ;;
        ds)
            # NDS game ID is at offset 0x0C, 4 bytes
            dd if="$rom_file" bs=1 skip=12 count=4 2>/dev/null | tr -d '\0'
            ;;
        n64)
            # N64 game ID is at offset 0x3B, 4 bytes (for .z64 format)
            dd if="$rom_file" bs=1 skip=59 count=4 2>/dev/null | tr -d '\0'
            ;;
        snes)
            # SNES doesn't have a standard ID, use filename hash
            echo "SNES$(echo "$rom_file" | md5sum | cut -c1-4 | tr 'a-f' 'A-F')"
            ;;
        nes)
            # NES doesn't have a standard ID, use filename hash
            echo "NES$(echo "$rom_file" | md5sum | cut -c1-4 | tr 'a-f' 'A-F')"
            ;;
        *)
            # Fallback: use filename hash
            echo "$(echo "$system$rom_file" | md5sum | cut -c1-8 | tr 'a-f' 'A-F')"
            ;;
    esac
}

get_game_title_from_rom() {
    local rom_file="$1"
    local system="$2"

    case "$system" in
        gba)
            # GBA title is at offset 0xA0, 12 bytes
            dd if="$rom_file" bs=1 skip=160 count=12 2>/dev/null | tr -d '\0' | xargs
            ;;
        ds)
            # NDS title is at offset 0x00, 12 bytes
            dd if="$rom_file" bs=1 skip=0 count=12 2>/dev/null | tr -d '\0' | xargs
            ;;
        n64)
            # N64 title is at offset 0x20, 20 bytes
            dd if="$rom_file" bs=1 skip=32 count=20 2>/dev/null | tr -d '\0' | xargs
            ;;
        *)
            # Fallback: use filename without extension
            basename "${rom_file%.*}"
            ;;
    esac
}

download_cover_art() {
    local game_id="$1"
    local system="$2"
    local output_file="$3"

    local gametdb_sys="${GAMETDB_SYSTEMS[$system]}"
    if [[ -z "$gametdb_sys" ]]; then
        log_warn "No GameTDB mapping for system: $system"
        return 1
    fi

    # Try different cover types
    local cover_types=("cover" "cover3D" "coverHQ" "disc")
    local regions=("US" "EN" "EU" "JA")

    for cover_type in "${cover_types[@]}"; do
        for region in "${regions[@]}"; do
            local url="https://art.gametdb.com/${gametdb_sys}/${cover_type}/${region}/${game_id}.png"

            if curl -sf -o "$output_file" "$url" 2>/dev/null; then
                log_success "Downloaded cover from GameTDB: $cover_type/$region"
                return 0
            fi
        done
    done

    log_warn "Could not find cover art on GameTDB for: $game_id"
    return 1
}

convert_to_tga() {
    local input_file="$1"
    local output_file="$2"
    local width="$3"
    local height="$4"
    local depth="${5:-24}"  # 24 for banners, 32 for icons

    if ! check_command convert; then
        log_error "ImageMagick not found. Run --setup first."
        return 1
    fi

    # Convert to TGA with correct dimensions and bit depth
    convert "$input_file" \
        -resize "${width}x${height}!" \
        -depth 8 \
        -type TrueColor \
        -alpha "${depth:=24}" \
        -compress none \
        "TGA:$output_file"

    log_success "Converted image to TGA: $(basename "$output_file")"
}

generate_placeholder_tga() {
    local output_file="$1"
    local width="$2"
    local height="$3"
    local text="$4"
    local depth="${5:-24}"

    if ! check_command convert; then
        log_error "ImageMagick not found"
        return 1
    fi

    # Create a simple placeholder with text
    convert -size "${width}x${height}" \
        xc:"#1E1E2E" \
        -fill "#CDD6F4" \
        -gravity center \
        -pointsize $((height / 8)) \
        -annotate 0 "$text" \
        -depth 8 \
        -compress none \
        "TGA:$output_file"
}

#------------------------------------------------------------------------------
# Meta.xml Generation
#------------------------------------------------------------------------------

generate_title_id() {
    local game_id="$1"
    local system="$2"

    # Generate a title ID based on system and game ID
    # Format: 0005000010XXXXXX
    # We use a hash to generate unique but reproducible IDs

    local hash
    hash=$(echo "${system}${game_id}" | md5sum | cut -c1-6 | tr 'a-f' 'A-F')

    echo "0005000010${hash}"
}

generate_meta_xml() {
    local output_file="$1"
    local title_id="$2"
    local title="$3"
    local publisher="${4:-Nintendo}"

    cat > "$output_file" << EOF
<?xml version="1.0" encoding="utf-8"?>
<menu>
  <title_id type="hexBinary" length="8">$title_id</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <common_save_size type="hexBinary" length="8">0000000000000000</common_save_size>
  <account_save_size type="hexBinary" length="8">0000000000000000</account_save_size>
  <longname_ja>$title</longname_ja>
  <longname_en>$title</longname_en>
  <longname_fr>$title</longname_fr>
  <longname_de>$title</longname_de>
  <longname_it>$title</longname_it>
  <longname_es>$title</longname_es>
  <longname_zhs>$title</longname_zhs>
  <longname_ko>$title</longname_ko>
  <longname_nl>$title</longname_nl>
  <longname_pt>$title</longname_pt>
  <longname_ru>$title</longname_ru>
  <longname_zht>$title</longname_zht>
  <shortname_ja>$title</shortname_ja>
  <shortname_en>$title</shortname_en>
  <shortname_fr>$title</shortname_fr>
  <shortname_de>$title</shortname_de>
  <shortname_it>$title</shortname_it>
  <shortname_es>$title</shortname_es>
  <shortname_zhs>$title</shortname_zhs>
  <shortname_ko>$title</shortname_ko>
  <shortname_nl>$title</shortname_nl>
  <shortname_pt>$title</shortname_pt>
  <shortname_ru>$title</shortname_ru>
  <shortname_zht>$title</shortname_zht>
  <publisher_ja>$publisher</publisher_ja>
  <publisher_en>$publisher</publisher_en>
  <publisher_fr>$publisher</publisher_fr>
  <publisher_de>$publisher</publisher_de>
  <publisher_it>$publisher</publisher_it>
  <publisher_es>$publisher</publisher_es>
  <publisher_zhs>$publisher</publisher_zhs>
  <publisher_ko>$publisher</publisher_ko>
  <publisher_nl>$publisher</publisher_nl>
  <publisher_pt>$publisher</publisher_pt>
  <publisher_ru>$publisher</publisher_ru>
  <publisher_zht>$publisher</publisher_zht>
</menu>
EOF
}

generate_app_xml() {
    local output_file="$1"
    local title_id="$2"

    cat > "$output_file" << EOF
<?xml version="1.0" encoding="utf-8"?>
<app type="complex" access="777">
  <title_id type="hexBinary" length="8">$title_id</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <app_type type="hexBinary" length="4">80000000</app_type>
</app>
EOF
}

#------------------------------------------------------------------------------
# System-Specific Injection
#------------------------------------------------------------------------------

inject_nes_snes() {
    local rom_file="$1"
    local base_dir="$2"
    local work_dir="$3"
    local system="$4"

    log_step "Injecting $system ROM..."

    # For NES/SNES, the ROM is embedded in the RPX file
    # Process: decompress RPX -> inject ROM -> recompress RPX

    # Check for wiiurpxtool
    if [[ ! -f "$TOOLS_DIR/wiiurpxtool" ]]; then
        die "wiiurpxtool not found. Run --setup first."
    fi

    # Check for Python 3
    if ! check_command python3; then
        die "Python 3 required for NES/SNES injection. Run --setup first."
    fi

    # Find the RPX file
    local rpx_file
    rpx_file=$(find "$base_dir/code" -name "*.rpx" | head -1)

    if [[ -z "$rpx_file" ]]; then
        die "No RPX file found in base game code/ folder"
    fi

    local rpx_name=$(basename "$rpx_file")
    log_info "Found RPX: $rpx_name"

    # Copy base to work directory
    cp -r "$base_dir/code" "$work_dir/"
    cp -r "$base_dir/content" "$work_dir/"
    cp -r "$base_dir/meta" "$work_dir/"

    local work_rpx="$work_dir/code/$rpx_name"

    # Step 1: Decompress RPX
    log_info "Decompressing RPX..."
    "$TOOLS_DIR/wiiurpxtool" -d "$work_rpx"

    if [[ $? -ne 0 ]]; then
        die "Failed to decompress RPX"
    fi

    # Step 2: Inject ROM using Python helper
    log_info "Injecting ROM into RPX..."

    local inject_script="$SCRIPT_DIR/rpx_inject.py"
    if [[ ! -f "$inject_script" ]]; then
        die "rpx_inject.py not found in script directory"
    fi

    # Run the injection
    if ! python3 "$inject_script" "$work_rpx" "$rom_file" "$system"; then
        log_error "ROM injection failed"
        log_info ""
        log_info "This may happen if:"
        log_info "  1. The ROM is larger than the base game's ROM"
        log_info "  2. The ROM offset couldn't be auto-detected"
        log_info ""
        log_info "To inject manually:"
        log_info "  1. Find the ROM offset in a hex editor (search for 'WUP-')"
        log_info "  2. Run: python3 rpx_inject.py $work_rpx $rom_file $system --offset 0xOFFSET"
        die "Injection failed"
    fi

    # Step 3: Recompress RPX
    log_info "Recompressing RPX..."
    "$TOOLS_DIR/wiiurpxtool" -c "$work_rpx"

    if [[ $? -ne 0 ]]; then
        die "Failed to recompress RPX"
    fi

    log_success "$system ROM injected successfully"
}

inject_gba() {
    local rom_file="$1"
    local base_dir="$2"
    local work_dir="$3"

    log_step "Injecting GBA ROM..."

    # Check for inject_gba
    if ! check_command inject_gba; then
        if [[ -f "$TOOLS_DIR/inject_gba/inject_gba.py" ]]; then
            local INJECT_GBA="python3 $TOOLS_DIR/inject_gba/inject_gba.py"
        else
            die "inject_gba not found. Run --setup first."
        fi
    else
        local INJECT_GBA="inject_gba"
    fi

    # Copy base to work directory
    cp -r "$base_dir/code" "$work_dir/"
    cp -r "$base_dir/content" "$work_dir/"
    cp -r "$base_dir/meta" "$work_dir/"

    # Find the PSB.m file
    local psb_file="$work_dir/content/alldata.psb.m"

    if [[ ! -f "$psb_file" ]]; then
        die "alldata.psb.m not found in base game content folder"
    fi

    # Inject the ROM
    log_info "Injecting ROM into alldata.psb.m..."
    $INJECT_GBA \
        --inpsb "$psb_file" \
        --inrom "$rom_file" \
        --outpsb "$psb_file.new" \
        --allow-overwrite

    mv "$psb_file.new" "$psb_file"

    log_success "GBA ROM injected successfully"
}

inject_ds() {
    local rom_file="$1"
    local base_dir="$2"
    local work_dir="$3"

    log_step "Injecting DS ROM..."

    # Copy base to work directory
    cp -r "$base_dir/code" "$work_dir/"
    cp -r "$base_dir/content" "$work_dir/"
    cp -r "$base_dir/meta" "$work_dir/"

    # DS ROMs go in content/0010/
    local rom_dir="$work_dir/content/0010"
    mkdir -p "$rom_dir"

    # Check if base uses rom.nds or rom.zip
    if [[ -f "$base_dir/content/0010/rom.zip" ]]; then
        # Create a zip containing the ROM
        log_info "Creating rom.zip..."
        local rom_basename=$(basename "$rom_file")
        local temp_rom="$TEMP_DIR/rom.nds"
        cp "$rom_file" "$temp_rom"
        cd "$TEMP_DIR"
        zip -j "$rom_dir/rom.zip" "rom.nds"
        cd "$SCRIPT_DIR"
    else
        # Just copy as rom.nds
        log_info "Copying ROM as rom.nds..."
        cp "$rom_file" "$rom_dir/rom.nds"
    fi

    log_success "DS ROM injected successfully"
}

inject_n64() {
    local rom_file="$1"
    local base_dir="$2"
    local work_dir="$3"
    local game_id="$4"

    log_step "Injecting N64 ROM..."

    # Copy base to work directory
    cp -r "$base_dir/code" "$work_dir/"
    cp -r "$base_dir/content" "$work_dir/"
    cp -r "$base_dir/meta" "$work_dir/"

    # N64 ROMs go in content/rom/
    local rom_dir="$work_dir/content/rom"
    mkdir -p "$rom_dir"

    # The ROM filename needs to match the game's expected name
    # This is typically based on the original game's internal name
    # For now, we'll use a generic name
    local rom_name="${game_id}.z64"

    # Convert ROM to .z64 format if needed (big-endian)
    local ext="${rom_file##*.}"
    if [[ "$ext" == "n64" ]] || [[ "$ext" == "v64" ]]; then
        log_info "Converting ROM to .z64 format..."
        # Simple byte swap for v64, word swap for n64
        # This is a simplified approach
        cp "$rom_file" "$rom_dir/$rom_name"
    else
        cp "$rom_file" "$rom_dir/$rom_name"
    fi

    # N64 also needs a config INI file
    local config_dir="$work_dir/content/config"
    mkdir -p "$config_dir"

    log_warn "N64 games may need a custom .ini config file for best compatibility"
    log_warn "Check: https://wiki.gbatemp.net/wiki/WiiU_VC_N64_inject_compatibility_list"

    log_success "N64 ROM injected (may need INI config for full compatibility)"
}

inject_tg16() {
    local rom_file="$1"
    local base_dir="$2"
    local work_dir="$3"

    log_step "Injecting TG16/PCE ROM..."

    # Copy base to work directory
    cp -r "$base_dir/code" "$work_dir/"
    cp -r "$base_dir/content" "$work_dir/"
    cp -r "$base_dir/meta" "$work_dir/"

    # TG16 is similar to NES/SNES - ROM embedded in RPX
    # This is a placeholder for the full implementation

    log_warn "TG16 injection requires RPX modification (similar to NES/SNES)"
    log_warn "This is a placeholder - full implementation pending"

    log_success "TG16 ROM injection structure created"
}

#------------------------------------------------------------------------------
# WUP Packaging
#------------------------------------------------------------------------------

package_wup() {
    local input_dir="$1"
    local output_dir="$2"
    local title_id="$3"

    log_step "Packaging as WUP..."

    if [[ ! -f "$TOOLS_DIR/NUSPacker.jar" ]]; then
        die "NUSPacker not found. Run --setup first."
    fi

    if [[ ! -f "$COMMON_KEY_FILE" ]]; then
        die "Common key not found at: $COMMON_KEY_FILE"
    fi

    local common_key
    common_key=$(cat "$COMMON_KEY_FILE" | tr -d '[:space:]')

    mkdir -p "$output_dir"

    java -jar "$TOOLS_DIR/NUSPacker.jar" \
        -in "$input_dir" \
        -out "$output_dir" \
        -encryptKeyWith "$common_key"

    log_success "WUP package created: $output_dir"
}

#------------------------------------------------------------------------------
# Main Injection Process
#------------------------------------------------------------------------------

do_inject() {
    local system="$1"
    local rom_file="$2"
    local base_dir="$3"
    local output_dir="$4"
    local custom_title="$5"
    local custom_cover="$6"
    local custom_title_id="$7"
    local no_artwork="$8"
    local dry_run="$9"

    # Validate inputs
    if [[ ! -f "$rom_file" ]]; then
        die "ROM file not found: $rom_file"
    fi

    if [[ ! -d "$base_dir/code" ]] || [[ ! -d "$base_dir/content" ]] || [[ ! -d "$base_dir/meta" ]]; then
        die "Invalid base game folder. Must contain code/, content/, and meta/ directories."
    fi

    # Get game info
    local game_id
    local game_title

    game_id=$(get_game_id_from_rom "$rom_file" "$system")
    game_title=$(get_game_title_from_rom "$rom_file" "$system")

    # Use custom title if provided
    if [[ -n "$custom_title" ]]; then
        game_title="$custom_title"
    fi

    # Clean up title
    game_title=$(echo "$game_title" | sed 's/[^a-zA-Z0-9 ._-]//g' | xargs)
    if [[ -z "$game_title" ]]; then
        game_title=$(basename "${rom_file%.*}")
    fi

    # Generate title ID
    local title_id
    if [[ -n "$custom_title_id" ]]; then
        title_id="$custom_title_id"
    else
        title_id=$(generate_title_id "$game_id" "$system")
    fi

    echo ""
    log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    log_info "Injecting: $(basename "$rom_file")"
    log_info "  System:   $system"
    log_info "  Game ID:  $game_id"
    log_info "  Title:    $game_title"
    log_info "  Title ID: $title_id"
    log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    if [[ "$dry_run" == "true" ]]; then
        log_info "[DRY RUN] Would inject and package"
        return 0
    fi

    # Create work directory
    local work_dir="$TEMP_DIR/$title_id"
    rm -rf "$work_dir"
    mkdir -p "$work_dir"

    # Perform system-specific injection
    case "$system" in
        nes|snes)
            inject_nes_snes "$rom_file" "$base_dir" "$work_dir" "$system"
            ;;
        gba)
            inject_gba "$rom_file" "$base_dir" "$work_dir"
            ;;
        ds)
            inject_ds "$rom_file" "$base_dir" "$work_dir"
            ;;
        n64)
            inject_n64 "$rom_file" "$base_dir" "$work_dir" "$game_id"
            ;;
        tg16)
            inject_tg16 "$rom_file" "$base_dir" "$work_dir"
            ;;
        wii|gc)
            die "For Wii/GameCube, use convert.sh instead"
            ;;
        *)
            die "Unknown system: $system"
            ;;
    esac

    # Update metadata
    log_step "Updating metadata..."
    generate_meta_xml "$work_dir/meta/meta.xml" "$title_id" "$game_title"
    generate_app_xml "$work_dir/code/app.xml" "$title_id"

    # Handle cover art
    log_step "Processing cover art..."
    local cover_file=""

    if [[ -n "$custom_cover" ]] && [[ -f "$custom_cover" ]]; then
        cover_file="$custom_cover"
    elif [[ "$no_artwork" != "true" ]]; then
        # Try to download from GameTDB
        local cached_cover="$CACHE_DIR/covers/${game_id}.png"
        if [[ -f "$cached_cover" ]]; then
            cover_file="$cached_cover"
            log_info "Using cached cover art"
        else
            mkdir -p "$CACHE_DIR/covers"
            if download_cover_art "$game_id" "$system" "$cached_cover"; then
                cover_file="$cached_cover"
            fi
        fi
    fi

    if [[ -n "$cover_file" ]]; then
        # Convert to TGA files
        convert_to_tga "$cover_file" "$work_dir/meta/iconTex.tga" 128 128 32
        convert_to_tga "$cover_file" "$work_dir/meta/bootTvTex.tga" 1280 720 24
        convert_to_tga "$cover_file" "$work_dir/meta/bootDrcTex.tga" 854 480 24
    else
        log_warn "No cover art available, using placeholders"
        generate_placeholder_tga "$work_dir/meta/iconTex.tga" 128 128 "$game_title" 32
        generate_placeholder_tga "$work_dir/meta/bootTvTex.tga" 1280 720 "$game_title" 24
        generate_placeholder_tga "$work_dir/meta/bootDrcTex.tga" 854 480 "$game_title" 24
    fi

    # Package as WUP
    local wup_output="$output_dir/$title_id"
    package_wup "$work_dir" "$wup_output" "$title_id"

    echo ""
    log_success "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    log_success "Injection complete!"
    log_info "Output: $wup_output"
    log_info ""
    log_info "To install on Wii U:"
    log_info "  1. Copy the output folder to SD:/install/"
    log_info "  2. Run WUP Installer GX2"
    log_success "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

#------------------------------------------------------------------------------
# Main Entry Point
#------------------------------------------------------------------------------

main() {
    local system=""
    local rom_file=""
    local base_dir=""
    local output_dir="$OUTPUT_DIR"
    local custom_title=""
    local custom_cover=""
    local custom_title_id=""
    local no_artwork=false
    local batch_mode=false
    local roms_dir=""
    local dry_run=false
    local keep_temp=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                show_help
                exit 0
                ;;
            --setup)
                do_setup
                exit 0
                ;;
            --setup-base)
                shift
                setup_base_game "$1" "$2"
                exit 0
                ;;
            --system|-s)
                shift
                system="$1"
                ;;
            --rom|-r)
                shift
                rom_file="$1"
                ;;
            --base|-b)
                shift
                base_dir="$1"
                ;;
            --output|-o)
                shift
                output_dir="$1"
                ;;
            --title|-t)
                shift
                custom_title="$1"
                ;;
            --title-id)
                shift
                custom_title_id="$1"
                ;;
            --cover)
                shift
                custom_cover="$1"
                ;;
            --no-artwork)
                no_artwork=true
                ;;
            --batch)
                batch_mode=true
                ;;
            --roms)
                shift
                roms_dir="$1"
                ;;
            --dry-run)
                dry_run=true
                ;;
            --keep-temp)
                keep_temp=true
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
        shift
    done

    # Validate system
    if [[ -z "$system" ]]; then
        show_help
        exit 1
    fi

    if [[ ! " $SUPPORTED_SYSTEMS " =~ " $system " ]]; then
        die "Unsupported system: $system. Supported: $SUPPORTED_SYSTEMS"
    fi

    # Validate base directory
    if [[ -z "$base_dir" ]]; then
        die "Base game directory required. Use --base <folder>"
    fi

    # Create temp directory
    mkdir -p "$TEMP_DIR"
    mkdir -p "$output_dir"

    if [[ "$batch_mode" == "true" ]]; then
        # Batch mode
        if [[ -z "$roms_dir" ]]; then
            die "ROMs directory required for batch mode. Use --roms <folder>"
        fi

        if [[ ! -d "$roms_dir" ]]; then
            die "ROMs directory not found: $roms_dir"
        fi

        log_info "Batch processing ROMs in: $roms_dir"

        local extensions="${ROM_EXTENSIONS[$system]}"
        local find_args=()
        for ext in $extensions; do
            find_args+=(-o -iname "*.$ext")
        done
        # Remove first -o
        unset 'find_args[0]'

        local rom_files
        mapfile -t rom_files < <(find "$roms_dir" -type f \( "${find_args[@]}" \) 2>/dev/null | sort)

        if [[ ${#rom_files[@]} -eq 0 ]]; then
            die "No ROM files found for system: $system"
        fi

        log_info "Found ${#rom_files[@]} ROM(s)"

        for rom in "${rom_files[@]}"; do
            do_inject "$system" "$rom" "$base_dir" "$output_dir" "" "" "" "$no_artwork" "$dry_run"
        done
    else
        # Single file mode
        if [[ -z "$rom_file" ]]; then
            die "ROM file required. Use --rom <file>"
        fi

        do_inject "$system" "$rom_file" "$base_dir" "$output_dir" \
            "$custom_title" "$custom_cover" "$custom_title_id" "$no_artwork" "$dry_run"
    fi

    # Cleanup
    if [[ "$keep_temp" != "true" ]] && [[ "$dry_run" != "true" ]]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$TEMP_DIR"
    fi

    echo ""
    log_success "All done!"
}

main "$@"
