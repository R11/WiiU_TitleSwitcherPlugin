#!/bin/bash
#
# SDUSB Setup Script for Wii U
# Sets up an SD card to play Wii U games at full speed using SDUSB
#
# Based on: https://gbatemp.net/threads/sdusb-the-modern-way-to-play-wii-u-games-from-sd-at-full-speed.655744/
# Requires: Aroma homebrew environment already set up
#
# IMPORTANT: This script will PARTITION your SD card. Back up all data first!
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# URLs for downloads
WAFEL_SD_USB_URL="https://github.com/StroopwafelCFW/wafel_sd_usb/releases/latest/download/wafel_sd_usb.ipx"
AROMA_URL="https://aroma.foryour.cafe/api/download?packages=environmentloader,wiiu-nanddumper-payload,payloadloaderinstaller,bloopair,ftpiiu,homebrew_on_menu,regionfree,wiiload,aroma-base,autoboot,sdcafiine,screenshot"

# Configuration
FAT32_SIZE_GB=32  # Size of FAT32 partition in GB (for homebrew, saves, etc.)
ALIGN_MB=64       # Alignment in MiB (recommended for SD cards)

print_header() {
    echo -e "${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════════╗"
    echo "║          SDUSB Setup Script for Wii U                         ║"
    echo "║   Play Wii U games from SD card at full speed                 ║"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠  $1${NC}"
}

print_error() {
    echo -e "${RED}✗  $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓  $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ  $1${NC}"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root (for partitioning)"
        echo "  Run with: sudo $0"
        exit 1
    fi
}

check_dependencies() {
    local missing=()

    for cmd in parted mkfs.fat mkfs.ntfs curl wget; do
        if ! command -v "$cmd" &> /dev/null; then
            missing+=("$cmd")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        print_error "Missing required tools: ${missing[*]}"
        echo ""
        echo "Install them with:"
        if command -v apt &> /dev/null; then
            echo "  sudo apt install parted dosfstools ntfs-3g curl wget"
        elif command -v pacman &> /dev/null; then
            echo "  sudo pacman -S parted dosfstools ntfs-3g curl wget"
        elif command -v dnf &> /dev/null; then
            echo "  sudo dnf install parted dosfstools ntfs-3g curl wget"
        elif command -v brew &> /dev/null; then
            echo "  brew install parted ntfs-3g curl wget"
            echo "  (Note: macOS users may need additional setup for NTFS)"
        fi
        exit 1
    fi

    print_success "All dependencies found"
}

list_sd_cards() {
    echo ""
    echo "Available removable devices:"
    echo "─────────────────────────────────────────────────────────────────"

    lsblk -d -o NAME,SIZE,MODEL,TRAN | grep -E "usb|mmc" || {
        print_warning "No removable devices found via USB or MMC"
        echo ""
        echo "All block devices:"
        lsblk -d -o NAME,SIZE,MODEL,TRAN
    }
    echo "─────────────────────────────────────────────────────────────────"
}

select_device() {
    list_sd_cards
    echo ""
    read -p "Enter the device name (e.g., sdb, mmcblk0): " DEVICE

    if [[ -z "$DEVICE" ]]; then
        print_error "No device specified"
        exit 1
    fi

    # Normalize device path
    if [[ ! "$DEVICE" =~ ^/dev/ ]]; then
        DEVICE="/dev/$DEVICE"
    fi

    if [[ ! -b "$DEVICE" ]]; then
        print_error "Device $DEVICE does not exist"
        exit 1
    fi

    # Get device size
    local size_bytes=$(blockdev --getsize64 "$DEVICE" 2>/dev/null)
    local size_gb=$((size_bytes / 1024 / 1024 / 1024))

    echo ""
    print_warning "Selected device: $DEVICE (${size_gb}GB)"
    print_warning "ALL DATA ON THIS DEVICE WILL BE ERASED!"
    echo ""
    read -p "Type 'YES' to confirm: " confirm

    if [[ "$confirm" != "YES" ]]; then
        print_info "Aborted"
        exit 0
    fi
}

unmount_device() {
    print_info "Unmounting all partitions on $DEVICE..."

    # Unmount all partitions
    for part in "${DEVICE}"*; do
        if mountpoint -q "$part" 2>/dev/null || mount | grep -q "^$part "; then
            umount "$part" 2>/dev/null || true
        fi
    done

    # For mmcblk devices
    for part in "${DEVICE}p"*; do
        if [[ -b "$part" ]]; then
            if mountpoint -q "$part" 2>/dev/null || mount | grep -q "^$part "; then
                umount "$part" 2>/dev/null || true
            fi
        fi
    done

    sleep 1
    print_success "Device unmounted"
}

partition_sd_card() {
    local device="$DEVICE"
    local size_bytes=$(blockdev --getsize64 "$device")
    local size_mb=$((size_bytes / 1024 / 1024))

    # Calculate partition sizes (aligned to 64MiB)
    local fat32_size_mb=$((FAT32_SIZE_GB * 1024))
    local fat32_start_mb=$ALIGN_MB
    local fat32_end_mb=$((fat32_start_mb + fat32_size_mb))

    # Round to alignment
    fat32_end_mb=$(( (fat32_end_mb / ALIGN_MB) * ALIGN_MB ))

    local ntfs_start_mb=$fat32_end_mb
    local ntfs_end_mb=$(( (size_mb / ALIGN_MB) * ALIGN_MB - ALIGN_MB ))

    print_info "Partitioning $device..."
    echo "  FAT32: ${fat32_start_mb}MiB - ${fat32_end_mb}MiB (~${FAT32_SIZE_GB}GB)"
    echo "  NTFS:  ${ntfs_start_mb}MiB - ${ntfs_end_mb}MiB (rest of card)"
    echo ""

    # Create MBR partition table
    parted -s "$device" mklabel msdos

    # Create FAT32 partition (primary, with LBA flag)
    parted -s "$device" mkpart primary fat32 "${fat32_start_mb}MiB" "${fat32_end_mb}MiB"
    parted -s "$device" set 1 lba on

    # Create NTFS partition (primary) - This will become WFS after Wii U formats it
    parted -s "$device" mkpart primary ntfs "${ntfs_start_mb}MiB" "${ntfs_end_mb}MiB"

    # Wait for kernel to recognize partitions
    sleep 2
    partprobe "$device" 2>/dev/null || true
    sleep 2

    print_success "Partitioning complete"
}

get_partition_name() {
    local device="$1"
    local num="$2"

    # Handle different naming conventions (sdb1 vs mmcblk0p1)
    if [[ "$device" =~ [0-9]$ ]]; then
        echo "${device}p${num}"
    else
        echo "${device}${num}"
    fi
}

format_partitions() {
    local part1=$(get_partition_name "$DEVICE" 1)
    local part2=$(get_partition_name "$DEVICE" 2)

    print_info "Formatting partitions..."

    # Format FAT32 with 32k cluster size (optimal for Wii U)
    print_info "Formatting $part1 as FAT32..."
    mkfs.fat -F 32 -s 64 -n "WIIU" "$part1"

    # Format NTFS (placeholder - Wii U will reformat as WFS)
    print_info "Formatting $part2 as NTFS (will be reformatted by Wii U as WFS)..."
    mkfs.ntfs -f -L "WIIU_USB" "$part2"

    print_success "Formatting complete"
}

setup_sd_structure() {
    local part1=$(get_partition_name "$DEVICE" 1)
    local mount_point="/tmp/wiiu_sd_setup_$$"

    mkdir -p "$mount_point"
    mount "$part1" "$mount_point"

    print_info "Creating directory structure..."

    # Create Aroma directory structure
    mkdir -p "$mount_point/wiiu/apps"
    mkdir -p "$mount_point/wiiu/environments/aroma/plugins"
    mkdir -p "$mount_point/wiiu/environments/aroma/modules/setup"
    mkdir -p "$mount_point/wiiu/ios_plugins"
    mkdir -p "$mount_point/wiiu/payloads"

    print_success "Directory structure created"

    # Download wafel_sd_usb plugin
    print_info "Downloading wafel_sd_usb plugin..."
    if curl -L -o "$mount_point/wiiu/ios_plugins/wafel_sd_usb.ipx" "$WAFEL_SD_USB_URL" 2>/dev/null; then
        print_success "Downloaded wafel_sd_usb.ipx"
    else
        print_warning "Could not download wafel_sd_usb.ipx - you'll need to download it manually"
        print_info "Get it from: https://github.com/StroopwafelCFW/wafel_sd_usb/releases"
    fi

    # Cleanup
    sync
    umount "$mount_point"
    rmdir "$mount_point"

    print_success "SD card structure set up"
}

download_only_mode() {
    local target_dir="$1"

    if [[ -z "$target_dir" ]]; then
        target_dir="./wiiu_sd_files"
    fi

    print_info "Download-only mode: saving files to $target_dir"

    mkdir -p "$target_dir/wiiu/apps"
    mkdir -p "$target_dir/wiiu/environments/aroma/plugins"
    mkdir -p "$target_dir/wiiu/environments/aroma/modules/setup"
    mkdir -p "$target_dir/wiiu/ios_plugins"
    mkdir -p "$target_dir/wiiu/payloads"

    print_info "Downloading wafel_sd_usb plugin..."
    if curl -L -o "$target_dir/wiiu/ios_plugins/wafel_sd_usb.ipx" "$WAFEL_SD_USB_URL" 2>/dev/null; then
        print_success "Downloaded wafel_sd_usb.ipx"
    else
        print_error "Could not download wafel_sd_usb.ipx"
    fi

    echo ""
    print_success "Files downloaded to: $target_dir"
    echo ""
    echo "Next steps:"
    echo "1. Partition your SD card manually with a FAT32 + NTFS partition"
    echo "2. Copy the contents of $target_dir to the FAT32 partition"
    echo "3. Set up Aroma if not already done (https://aroma.foryour.cafe/)"
    echo "4. Boot your Wii U - it will format the NTFS partition as WFS"
}

print_next_steps() {
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}Setup Complete!${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Next steps:"
    echo ""
    echo "1. ${YELLOW}Set up Aroma${NC} (if not already done):"
    echo "   - Visit: https://aroma.foryour.cafe/"
    echo "   - Download the base Aroma files"
    echo "   - Extract to the FAT32 partition of your SD card"
    echo ""
    echo "2. ${YELLOW}Set up ISFShax${NC} (recommended for permanent setup):"
    echo "   - Follow: https://wiiu.hacks.guide/"
    echo "   - This allows SDUSB to work on every boot"
    echo ""
    echo "3. ${YELLOW}First boot${NC}:"
    echo "   - Insert SD card into Wii U"
    echo "   - Boot with Aroma/ISFShax active"
    echo "   - The Wii U will detect the NTFS partition and ask to format it"
    echo "   - ${RED}IMPORTANT: Confirm the format!${NC} This creates the WFS filesystem"
    echo ""
    echo "4. ${YELLOW}Install games${NC}:"
    echo "   - Use WUP Installer GX2 to install games to 'USB' storage"
    echo "   - Games will actually be stored on your SD card's second partition"
    echo ""
    echo "5. ${YELLOW}Optional: Install TitleSwitcherPlugin${NC}:"
    echo "   - Copy TitleSwitcherPlugin.wps to /wiiu/environments/aroma/plugins/"
    echo "   - Press L+R+Minus to open the quick launcher"
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Useful resources:"
    echo "  - SDUSB Guide: https://gbatemp.net/threads/655744/"
    echo "  - wafel_sd_usb: https://github.com/StroopwafelCFW/wafel_sd_usb"
    echo "  - Wii U Hacks Guide: https://wiiu.hacks.guide/"
    echo "  - Aroma: https://aroma.foryour.cafe/"
    echo ""
}

print_manual_instructions() {
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}Manual Partition Instructions (Windows/macOS)${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "If you can't run this script, partition your SD card manually:"
    echo ""
    echo "${YELLOW}Windows:${NC}"
    echo "  1. Download MiniTool Partition Wizard (free) or similar"
    echo "  2. Delete all partitions on the SD card"
    echo "  3. Create partition table: MBR (not GPT!)"
    echo "  4. Create Partition 1: FAT32, ~32GB, Primary"
    echo "  5. Create Partition 2: NTFS, remaining space, Primary"
    echo "  6. Apply changes"
    echo ""
    echo "${YELLOW}macOS:${NC}"
    echo "  1. Open Disk Utility"
    echo "  2. Select SD card, click Erase"
    echo "  3. Format: MS-DOS (FAT), Scheme: Master Boot Record"
    echo "  4. Use diskutil in Terminal for more control:"
    echo "     diskutil partitionDisk /dev/diskN MBR \\"
    echo "       FAT32 WIIU 32G \\"
    echo "       ExFAT WIIU_USB R"
    echo "  (Note: You may need Paragon NTFS for NTFS support)"
    echo ""
    echo "${YELLOW}Important notes:${NC}"
    echo "  - Use MBR partition table, NOT GPT"
    echo "  - First partition MUST be FAT32"
    echo "  - Second partition can be NTFS or ExFAT (Wii U will reformat it)"
    echo "  - Align partitions to 64MiB boundaries for best performance"
    echo ""
}

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --help, -h          Show this help message"
    echo "  --download-only     Only download files (don't partition)"
    echo "  --target DIR        Target directory for download-only mode"
    echo "  --fat32-size GB     Size of FAT32 partition in GB (default: 32)"
    echo "  --manual            Show manual partitioning instructions"
    echo ""
    echo "Examples:"
    echo "  sudo $0                        # Interactive mode"
    echo "  sudo $0 --fat32-size 64        # Use 64GB for FAT32 partition"
    echo "  $0 --download-only             # Just download required files"
    echo "  $0 --manual                    # Show Windows/macOS instructions"
}

# Parse arguments
DOWNLOAD_ONLY=false
TARGET_DIR=""
SHOW_MANUAL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --help|-h)
            show_help
            exit 0
            ;;
        --download-only)
            DOWNLOAD_ONLY=true
            shift
            ;;
        --target)
            TARGET_DIR="$2"
            shift 2
            ;;
        --fat32-size)
            FAT32_SIZE_GB="$2"
            shift 2
            ;;
        --manual)
            SHOW_MANUAL=true
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Main execution
print_header

if [[ "$SHOW_MANUAL" == true ]]; then
    print_manual_instructions
    exit 0
fi

if [[ "$DOWNLOAD_ONLY" == true ]]; then
    download_only_mode "$TARGET_DIR"
    print_next_steps
    exit 0
fi

# Full setup mode
check_root
check_dependencies
select_device
unmount_device
partition_sd_card
format_partitions
setup_sd_structure
print_next_steps
