#!/bin/bash
#
# Wii/GameCube Disc to Wii U Converter
#
# Converts GameCube and Wii disc images to Wii U installable WUP format
# using Linux-native tools and Wine for the injection step.
#
# Requirements:
#   - wit (Wiimms ISO Tools) - for disc image conversion
#   - wine - for running TeconMoon's WiiVC Injector
#   - java - for NUSPacker
#   - p7zip - for extracting archives
#
# Usage:
#   ./convert.sh <input_folder> [output_folder]
#   ./convert.sh --setup              # Install dependencies
#   ./convert.sh --help               # Show help
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/deps"
CACHE_DIR="$SCRIPT_DIR/.cache"
TEMP_DIR="$SCRIPT_DIR/.tmp"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Tool paths (populated by check_dependencies)
WIT_BIN=""
WINE_BIN=""
JAVA_BIN=""
INJECTOR_DIR=""
NUSPACKER_JAR=""

# Configuration
COMMON_KEY_FILE="$SCRIPT_DIR/common_key.txt"
NINTENDONT_DOL=""

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

die() {
    log_error "$1"
    exit 1
}

show_help() {
    cat << 'EOF'
Wii/GameCube Disc to Wii U Converter
=====================================

Converts GameCube and Wii disc images to Wii U installable WUP format.

USAGE:
    ./convert.sh <input_folder> [output_folder]
    ./convert.sh --single <disc_image> [output_folder]
    ./convert.sh --setup
    ./convert.sh --setup-wine
    ./convert.sh --help

ARGUMENTS:
    input_folder     Folder containing disc images to convert
    output_folder    Where to save WUP packages (default: ./output)

OPTIONS:
    --single         Convert a single disc image instead of a folder
    --setup          Install required dependencies (wit, java, etc.)
    --setup-wine     Set up Wine and download TeconMoon's Injector
    --gc-only        Only process GameCube games
    --wii-only       Only process Wii games
    --skip-convert   Skip format conversion (assume all files are .iso)
    --dry-run        Show what would be done without doing it
    --help           Show this help message

SUPPORTED INPUT FORMATS:
    GameCube: .iso, .gcm, .nkit.iso, .ciso, .gcz
    Wii:      .iso, .wbfs, .nkit.iso, .ciso, .wia, .rvz

REQUIREMENTS:
    - wit (Wiimms ISO Tools) - for disc format conversion
    - wine + .NET Framework - for TeconMoon's WiiVC Injector
    - java 8+ - for NUSPacker
    - Wii U common key (place in common_key.txt)
    - Nintendont (downloaded automatically or manually placed)

EXAMPLES:
    # Set up dependencies first
    ./convert.sh --setup
    ./convert.sh --setup-wine

    # Convert all games in a folder
    ./convert.sh /path/to/roms /path/to/output

    # Convert a single game
    ./convert.sh --single "Super Mario Sunshine.iso"

NOTES:
    - GameCube games require Nintendont to be installed on your Wii U
    - The common key is required for NUSPacker to create installable packages
    - First-time Wine setup may take a while to install .NET Framework

For more information, see:
    - https://github.com/piratesephiroth/TeconmoonWiiVCInjector
    - https://wit.wiimm.de/
    - https://github.com/ihaveamac/nuspacker

EOF
}

#------------------------------------------------------------------------------
# Dependency Management
#------------------------------------------------------------------------------

check_command() {
    command -v "$1" &> /dev/null
}

install_wit() {
    log_info "Installing Wiimms ISO Tools (wit)..."

    if check_command apt-get; then
        sudo apt-get update && sudo apt-get install -y wit
    elif check_command pacman; then
        sudo pacman -S --noconfirm wiimms-iso-tools
    elif check_command dnf; then
        sudo dnf install -y wit
    elif check_command brew; then
        brew install wit
    else
        # Manual installation
        log_info "Package manager not detected, installing manually..."
        mkdir -p "$TOOLS_DIR"
        cd "$TOOLS_DIR"

        local WIT_URL="https://wit.wiimm.de/download/wit-v3.05a-r8638-x86_64.tar.gz"
        curl -L "$WIT_URL" -o wit.tar.gz
        tar -xzf wit.tar.gz
        cd wit-*/
        sudo ./install.sh
        cd "$SCRIPT_DIR"
    fi

    log_success "wit installed successfully"
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

    log_success "Java installed successfully"
}

install_wine() {
    log_info "Installing Wine..."

    if check_command apt-get; then
        sudo dpkg --add-architecture i386
        sudo apt-get update
        sudo apt-get install -y wine wine32 wine64 winetricks
    elif check_command pacman; then
        sudo pacman -S --noconfirm wine winetricks
    elif check_command dnf; then
        sudo dnf install -y wine winetricks
    elif check_command brew; then
        brew install --cask wine-stable
        brew install winetricks
    else
        die "Could not install Wine. Please install Wine manually."
    fi

    log_success "Wine installed successfully"
}

download_nuspacker() {
    log_info "Downloading NUSPacker..."
    mkdir -p "$TOOLS_DIR"

    local NUSPACKER_URL="https://github.com/ihaveamac/nuspacker/releases/download/v1.2/NUSPacker.jar"
    curl -L "$NUSPACKER_URL" -o "$TOOLS_DIR/NUSPacker.jar"

    log_success "NUSPacker downloaded to $TOOLS_DIR/NUSPacker.jar"
}

download_injector() {
    log_info "Downloading TeconMoon's WiiVC Injector..."
    mkdir -p "$TOOLS_DIR"

    local INJECTOR_URL="https://github.com/piratesephiroth/TeconmoonWiiVCInjector/releases/download/v1.5.4/TeconMoons_WiiVC_Injector_1.5.4.zip"
    curl -L "$INJECTOR_URL" -o "$TOOLS_DIR/injector.zip"

    cd "$TOOLS_DIR"
    unzip -o injector.zip -d WiiVCInjector
    cd "$SCRIPT_DIR"

    log_success "WiiVC Injector downloaded to $TOOLS_DIR/WiiVCInjector"
}

download_nintendont() {
    log_info "Downloading Nintendont..."
    mkdir -p "$TOOLS_DIR/nintendont"

    local NINTENDONT_URL="https://github.com/FIX94/Nintendont/releases/latest/download/loader.dol"
    curl -L "$NINTENDONT_URL" -o "$TOOLS_DIR/nintendont/boot.dol"

    log_success "Nintendont downloaded to $TOOLS_DIR/nintendont/boot.dol"
}

setup_wine_dotnet() {
    log_info "Setting up Wine with .NET Framework..."
    log_info "This may take 10-15 minutes on first run..."

    # Use a dedicated Wine prefix for this tool
    export WINEPREFIX="$TOOLS_DIR/wine_prefix"
    export WINEARCH=win64

    mkdir -p "$WINEPREFIX"

    # Initialize wine prefix
    wineboot --init

    # Install .NET Framework using winetricks
    log_info "Installing .NET Framework 4.6.1 (required for WiiVC Injector)..."
    winetricks -q dotnet461

    log_success "Wine .NET setup complete"
}

do_setup() {
    log_info "Setting up Wii/GameCube to Wii U converter..."

    # Check and install wit
    if ! check_command wit; then
        install_wit
    else
        log_success "wit is already installed"
    fi

    # Check and install java
    if ! check_command java; then
        install_java
    else
        log_success "Java is already installed"
    fi

    # Download NUSPacker if not present
    if [[ ! -f "$TOOLS_DIR/NUSPacker.jar" ]]; then
        download_nuspacker
    else
        log_success "NUSPacker already present"
    fi

    # Download Nintendont if not present
    if [[ ! -f "$TOOLS_DIR/nintendont/boot.dol" ]]; then
        download_nintendont
    else
        log_success "Nintendont already present"
    fi

    log_success "Basic setup complete!"
    log_info ""
    log_info "Next steps:"
    log_info "  1. Run './convert.sh --setup-wine' to set up Wine + WiiVC Injector"
    log_info "  2. Place your Wii U common key in: $COMMON_KEY_FILE"
    log_info "     (32 hex characters, no spaces)"
}

do_setup_wine() {
    log_info "Setting up Wine and TeconMoon's WiiVC Injector..."

    # Check and install wine
    if ! check_command wine; then
        install_wine
    else
        log_success "Wine is already installed"
    fi

    # Download injector
    if [[ ! -d "$TOOLS_DIR/WiiVCInjector" ]]; then
        download_injector
    else
        log_success "WiiVC Injector already present"
    fi

    # Setup Wine with .NET
    setup_wine_dotnet

    log_success "Wine setup complete!"
}

check_dependencies() {
    local missing=0

    # Check wit
    if check_command wit; then
        WIT_BIN="$(command -v wit)"
        log_success "Found wit: $WIT_BIN"
    else
        log_error "wit not found. Run './convert.sh --setup'"
        missing=1
    fi

    # Check java
    if check_command java; then
        JAVA_BIN="$(command -v java)"
        log_success "Found java: $JAVA_BIN"
    else
        log_error "java not found. Run './convert.sh --setup'"
        missing=1
    fi

    # Check NUSPacker
    if [[ -f "$TOOLS_DIR/NUSPacker.jar" ]]; then
        NUSPACKER_JAR="$TOOLS_DIR/NUSPacker.jar"
        log_success "Found NUSPacker: $NUSPACKER_JAR"
    else
        log_error "NUSPacker not found. Run './convert.sh --setup'"
        missing=1
    fi

    # Check wine (optional but recommended)
    if check_command wine; then
        WINE_BIN="$(command -v wine)"
        log_success "Found wine: $WINE_BIN"
    else
        log_warn "wine not found. Run './convert.sh --setup-wine' for full functionality"
    fi

    # Check injector
    if [[ -d "$TOOLS_DIR/WiiVCInjector" ]]; then
        INJECTOR_DIR="$TOOLS_DIR/WiiVCInjector"
        log_success "Found WiiVC Injector: $INJECTOR_DIR"
    else
        log_warn "WiiVC Injector not found. Run './convert.sh --setup-wine'"
    fi

    # Check Nintendont
    if [[ -f "$TOOLS_DIR/nintendont/boot.dol" ]]; then
        NINTENDONT_DOL="$TOOLS_DIR/nintendont/boot.dol"
        log_success "Found Nintendont: $NINTENDONT_DOL"
    else
        log_warn "Nintendont not found. Run './convert.sh --setup'"
    fi

    # Check common key
    if [[ -f "$COMMON_KEY_FILE" ]]; then
        log_success "Found common key file"
    else
        log_warn "Common key not found at: $COMMON_KEY_FILE"
        log_warn "NUSPacker will not work without it"
    fi

    return $missing
}

#------------------------------------------------------------------------------
# Disc Detection and Conversion
#------------------------------------------------------------------------------

detect_disc_type() {
    local file="$1"
    local ext="${file##*.}"
    local basename="${file%.*}"

    # Check for .nkit.iso
    if [[ "$basename" == *.nkit ]]; then
        ext="nkit.iso"
    fi

    # Use wit to detect if it's Wii or GameCube
    local info
    info=$("$WIT_BIN" id6 "$file" 2>/dev/null) || true

    if [[ -z "$info" ]]; then
        echo "unknown"
        return
    fi

    # GameCube IDs start with G, D, P, or R followed by specific patterns
    # Wii IDs can also start with R, S, etc.
    # Best way is to check the disc type via wit
    local disc_type
    disc_type=$("$WIT_BIN" dump "$file" 2>/dev/null | grep -i "disc type" | head -1) || true

    if echo "$disc_type" | grep -qi "gamecube"; then
        echo "gamecube"
    elif echo "$disc_type" | grep -qi "wii"; then
        echo "wii"
    else
        # Fallback: check file size (GameCube max ~1.4GB, Wii max ~8.5GB)
        local size
        size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
        if [[ $size -lt 1500000000 ]]; then
            echo "gamecube"
        else
            echo "wii"
        fi
    fi
}

get_game_id() {
    local file="$1"
    "$WIT_BIN" id6 "$file" 2>/dev/null | head -1
}

get_game_title() {
    local file="$1"
    "$WIT_BIN" dump "$file" 2>/dev/null | grep -i "disc title" | head -1 | sed 's/.*: *//'
}

convert_to_iso() {
    local input="$1"
    local output_dir="$2"
    local ext="${input##*.}"
    local basename="$(basename "${input%.*}")"

    # Handle .nkit.iso extension
    if [[ "$basename" == *.nkit ]]; then
        basename="${basename%.nkit}"
    fi

    local output_file="$output_dir/$basename.iso"

    # Skip if already ISO and not NKIT
    if [[ "$ext" == "iso" ]] && [[ ! "$input" == *".nkit."* ]]; then
        log_info "File is already ISO format: $basename"
        echo "$input"
        return
    fi

    log_info "Converting to ISO: $basename.$ext"

    mkdir -p "$output_dir"

    "$WIT_BIN" copy "$input" --iso "$output_file" \
        --psel data \
        --progress

    echo "$output_file"
}

#------------------------------------------------------------------------------
# Injection Process
#------------------------------------------------------------------------------

run_wiivc_injector() {
    local iso_file="$1"
    local output_dir="$2"
    local disc_type="$3"
    local game_id="$4"
    local game_title="$5"

    if [[ -z "$WINE_BIN" ]] || [[ ! -d "$INJECTOR_DIR" ]]; then
        log_error "Wine or WiiVC Injector not set up. Run './convert.sh --setup-wine'"
        return 1
    fi

    export WINEPREFIX="$TOOLS_DIR/wine_prefix"

    local injector_exe
    injector_exe=$(find "$INJECTOR_DIR" -name "*.exe" | head -1)

    if [[ -z "$injector_exe" ]]; then
        log_error "Could not find WiiVC Injector executable"
        return 1
    fi

    log_info "Launching WiiVC Injector for: $game_title"
    log_warn "This requires manual interaction in the GUI"
    log_info ""
    log_info "Instructions:"
    log_info "  1. Select '$disc_type Retail Game Injection' at the top"
    log_info "  2. Click 'Source Wii/GC Game' and select: $iso_file"
    log_info "  3. Configure any desired settings"
    log_info "  4. Set output directory to: $output_dir"
    log_info "  5. Select 'WUP Installable' format"
    log_info "  6. Click 'Build' or 'Inject'"
    log_info ""

    "$WINE_BIN" "$injector_exe" &
    local wine_pid=$!

    log_info "Press Enter when injection is complete..."
    read -r

    # Kill wine process if still running
    kill "$wine_pid" 2>/dev/null || true

    return 0
}

# Alternative: Command-line based injection for simple forwarders
create_nintendont_forwarder() {
    local gc_iso="$1"
    local output_dir="$2"
    local game_id="$3"
    local game_title="$4"

    log_info "Creating Nintendont forwarder for: $game_title"

    # This creates a simple forwarder that tells Nintendont to load the game
    # The actual GameCube ISO needs to be placed on the SD card

    local work_dir="$TEMP_DIR/$game_id"
    mkdir -p "$work_dir/code"
    mkdir -p "$work_dir/content"
    mkdir -p "$work_dir/meta"

    # Copy game ISO to content folder
    log_info "Copying game ISO to content folder..."
    cp "$gc_iso" "$work_dir/content/game.iso"

    # Create meta.xml
    cat > "$work_dir/meta/meta.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<menu>
  <title_id type="hexBinary" length="8">0005000010${game_id:0:6}</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <common_save_size type="hexBinary" length="8">0000000000000000</common_save_size>
  <account_save_size type="hexBinary" length="8">0000000000000000</account_save_size>
  <longname_en>$game_title</longname_en>
  <shortname_en>$game_title</shortname_en>
  <publisher_en>Nintendo</publisher_en>
</menu>
EOF

    # Create app.xml
    cat > "$work_dir/code/app.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<app type="complex" access="777">
  <title_id type="hexBinary" length="8">0005000010${game_id:0:6}</title_id>
  <title_version type="unsignedInt" length="4">0</title_version>
  <os_version type="hexBinary" length="8">000500101000400A</os_version>
  <app_type type="hexBinary" length="4">80000000</app_type>
</app>
EOF

    log_warn "Note: This creates a basic channel structure."
    log_warn "For full functionality, use TeconMoon's WiiVC Injector via Wine."

    echo "$work_dir"
}

#------------------------------------------------------------------------------
# WUP Packaging
#------------------------------------------------------------------------------

package_wup() {
    local input_dir="$1"
    local output_dir="$2"
    local game_id="$3"

    log_info "Packaging as WUP: $game_id"

    if [[ ! -f "$NUSPACKER_JAR" ]]; then
        log_error "NUSPacker not found"
        return 1
    fi

    if [[ ! -f "$COMMON_KEY_FILE" ]]; then
        log_error "Common key file not found: $COMMON_KEY_FILE"
        log_info "Please create this file with your Wii U common key (32 hex chars)"
        return 1
    fi

    local common_key
    common_key=$(cat "$COMMON_KEY_FILE" | tr -d '[:space:]')

    mkdir -p "$output_dir"

    "$JAVA_BIN" -jar "$NUSPACKER_JAR" \
        -in "$input_dir" \
        -out "$output_dir/$game_id" \
        -encryptKeyWith "$common_key"

    log_success "WUP package created: $output_dir/$game_id"
}

#------------------------------------------------------------------------------
# Batch Processing
#------------------------------------------------------------------------------

find_disc_images() {
    local dir="$1"
    local gc_only="$2"
    local wii_only="$3"

    # Find all supported disc image formats
    find "$dir" -type f \( \
        -iname "*.iso" -o \
        -iname "*.gcm" -o \
        -iname "*.wbfs" -o \
        -iname "*.ciso" -o \
        -iname "*.wia" -o \
        -iname "*.rvz" -o \
        -iname "*.gcz" -o \
        -iname "*.nkit.iso" \
    \) 2>/dev/null | sort
}

process_disc() {
    local file="$1"
    local output_dir="$2"
    local gc_only="$3"
    local wii_only="$4"
    local skip_convert="$5"
    local dry_run="$6"

    local game_id
    local game_title
    local disc_type
    local iso_file

    log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    log_info "Processing: $(basename "$file")"

    # Get game info
    game_id=$(get_game_id "$file")
    game_title=$(get_game_title "$file")
    disc_type=$(detect_disc_type "$file")

    log_info "  ID: $game_id"
    log_info "  Title: $game_title"
    log_info "  Type: $disc_type"

    # Filter by type
    if [[ "$gc_only" == "true" ]] && [[ "$disc_type" != "gamecube" ]]; then
        log_info "  Skipping (not GameCube)"
        return 0
    fi

    if [[ "$wii_only" == "true" ]] && [[ "$disc_type" != "wii" ]]; then
        log_info "  Skipping (not Wii)"
        return 0
    fi

    if [[ "$dry_run" == "true" ]]; then
        log_info "  [DRY RUN] Would convert and inject"
        return 0
    fi

    # Convert to ISO if needed
    if [[ "$skip_convert" != "true" ]]; then
        iso_file=$(convert_to_iso "$file" "$TEMP_DIR/iso")
    else
        iso_file="$file"
    fi

    # Run injection
    if [[ -n "$WINE_BIN" ]] && [[ -d "$INJECTOR_DIR" ]]; then
        run_wiivc_injector "$iso_file" "$output_dir" "$disc_type" "$game_id" "$game_title"
    else
        log_warn "Wine/Injector not available, creating basic structure..."
        local work_dir
        work_dir=$(create_nintendont_forwarder "$iso_file" "$output_dir" "$game_id" "$game_title")

        if [[ -f "$COMMON_KEY_FILE" ]]; then
            package_wup "$work_dir" "$output_dir" "$game_id"
        else
            log_warn "Common key not available, skipping WUP packaging"
            log_info "Output folder: $work_dir"
        fi
    fi

    log_success "Completed: $game_title"
}

#------------------------------------------------------------------------------
# Main Entry Point
#------------------------------------------------------------------------------

main() {
    local input=""
    local output_dir="$SCRIPT_DIR/output"
    local single_mode=false
    local gc_only=false
    local wii_only=false
    local skip_convert=false
    local dry_run=false

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
            --setup-wine)
                do_setup_wine
                exit 0
                ;;
            --single)
                single_mode=true
                shift
                ;;
            --gc-only)
                gc_only=true
                shift
                ;;
            --wii-only)
                wii_only=true
                shift
                ;;
            --skip-convert)
                skip_convert=true
                shift
                ;;
            --dry-run)
                dry_run=true
                shift
                ;;
            -*)
                die "Unknown option: $1"
                ;;
            *)
                if [[ -z "$input" ]]; then
                    input="$1"
                else
                    output_dir="$1"
                fi
                shift
                ;;
        esac
    done

    # Show help if no arguments
    if [[ -z "$input" ]]; then
        show_help
        exit 1
    fi

    # Check dependencies
    echo ""
    log_info "Checking dependencies..."
    if ! check_dependencies; then
        echo ""
        die "Missing required dependencies. Run './convert.sh --setup' first."
    fi
    echo ""

    # Create temp and output directories
    mkdir -p "$TEMP_DIR"
    mkdir -p "$output_dir"

    if [[ "$single_mode" == "true" ]]; then
        # Process single file
        if [[ ! -f "$input" ]]; then
            die "File not found: $input"
        fi
        process_disc "$input" "$output_dir" "$gc_only" "$wii_only" "$skip_convert" "$dry_run"
    else
        # Process folder
        if [[ ! -d "$input" ]]; then
            die "Directory not found: $input"
        fi

        log_info "Scanning for disc images in: $input"

        local files
        mapfile -t files < <(find_disc_images "$input" "$gc_only" "$wii_only")

        if [[ ${#files[@]} -eq 0 ]]; then
            die "No disc images found in: $input"
        fi

        log_info "Found ${#files[@]} disc image(s)"
        echo ""

        for file in "${files[@]}"; do
            process_disc "$file" "$output_dir" "$gc_only" "$wii_only" "$skip_convert" "$dry_run"
            echo ""
        done
    fi

    # Cleanup
    if [[ "$dry_run" != "true" ]]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$TEMP_DIR"
    fi

    log_success "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    log_success "All done!"
    log_info "WUP packages saved to: $output_dir"
    log_info ""
    log_info "To install on Wii U:"
    log_info "  1. Copy the output folders to SD:/install/"
    log_info "  2. Run WUP Installer GX2 on your Wii U"
    log_info "  3. For GameCube games, ensure Nintendont is at SD:/apps/nintendont/"
}

main "$@"
