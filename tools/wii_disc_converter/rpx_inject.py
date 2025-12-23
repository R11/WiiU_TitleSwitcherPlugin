#!/usr/bin/env python3
"""
SNES/NES ROM Injector for Wii U Virtual Console

This script handles the binary manipulation needed to inject ROMs
into decompressed Wii U VC RPX files.

Usage:
    python3 rpx_inject.py <rpx_file> <rom_file> <system> [--offset OFFSET]

Where:
    rpx_file    Decompressed RPX file (use wiiurpxtool -d first)
    rom_file    SNES (.sfc/.smc) or NES (.nes) ROM file
    system      'snes' or 'nes'
    --offset    Manual ROM offset (optional, auto-detected if not provided)
"""

import argparse
import os
import struct
import sys

# SNES ROM header signature locations (for LoROM and HiROM)
SNES_LOROM_HEADER = 0x7FC0  # LoROM header location
SNES_HIROM_HEADER = 0xFFC0  # HiROM header location

# WUP header signature
WUP_SIGNATURE = b'WUP-'

# SNES internal header offset within the header block
SNES_TITLE_OFFSET = 0x00      # 21 bytes - game title
SNES_MAKEUP_OFFSET = 0x15     # Map mode / ROM speed
SNES_TYPE_OFFSET = 0x16       # Cartridge type
SNES_ROMSIZE_OFFSET = 0x17    # ROM size
SNES_RAMSIZE_OFFSET = 0x18    # RAM size
SNES_COUNTRY_OFFSET = 0x19    # Country code
SNES_DEVID_OFFSET = 0x1A      # Developer ID
SNES_VERSION_OFFSET = 0x1B    # Version
SNES_CHECKSUM_COMP_OFFSET = 0x1C  # Checksum complement
SNES_CHECKSUM_OFFSET = 0x1E   # Checksum


def log_info(msg):
    print(f"\033[0;34m[INFO]\033[0m {msg}")


def log_success(msg):
    print(f"\033[0;32m[OK]\033[0m {msg}")


def log_warn(msg):
    print(f"\033[1;33m[WARN]\033[0m {msg}")


def log_error(msg):
    print(f"\033[0;31m[ERROR]\033[0m {msg}")


def read_file(filepath):
    """Read entire file as bytes."""
    with open(filepath, 'rb') as f:
        return bytearray(f.read())


def write_file(filepath, data):
    """Write bytes to file."""
    with open(filepath, 'wb') as f:
        f.write(data)


def strip_smc_header(rom_data):
    """
    Strip the 512-byte SMC header if present.
    SMC headers are identified by file size being 512 bytes more than a power of 2.
    """
    size = len(rom_data)

    # Check if size suggests an SMC header (size % 1024 == 512)
    if size % 1024 == 512:
        log_info("Detected SMC header (512 bytes), stripping...")
        return rom_data[512:]

    return rom_data


def validate_snes_header(data, offset):
    """
    Check if there's a valid SNES header at the given offset.
    Returns True if it looks like a valid header.
    """
    if offset + 0x20 > len(data):
        return False

    header = data[offset:offset + 0x20]

    # Check the checksum/complement relationship
    checksum = struct.unpack('<H', header[SNES_CHECKSUM_OFFSET:SNES_CHECKSUM_OFFSET+2])[0]
    complement = struct.unpack('<H', header[SNES_CHECKSUM_COMP_OFFSET:SNES_CHECKSUM_COMP_OFFSET+2])[0]

    # Checksum + complement should equal 0xFFFF
    if (checksum + complement) & 0xFFFF == 0xFFFF:
        return True

    # Also check if the title area contains printable ASCII
    title = header[SNES_TITLE_OFFSET:SNES_TITLE_OFFSET+21]
    printable = sum(1 for b in title if 0x20 <= b <= 0x7E or b == 0x00)
    if printable >= 15:  # At least 15 printable chars
        return True

    return False


def detect_snes_rom_type(rom_data):
    """
    Detect if ROM is LoROM or HiROM.
    Returns 'lorom', 'hirom', or None if can't detect.
    """
    # Check LoROM header location
    if len(rom_data) > SNES_LOROM_HEADER + 0x20:
        if validate_snes_header(rom_data, SNES_LOROM_HEADER):
            return 'lorom'

    # Check HiROM header location
    if len(rom_data) > SNES_HIROM_HEADER + 0x20:
        if validate_snes_header(rom_data, SNES_HIROM_HEADER):
            return 'hirom'

    return None


def find_wup_header(rpx_data):
    """
    Find the WUP header in the RPX file.
    Returns offset or -1 if not found.
    """
    offset = rpx_data.find(WUP_SIGNATURE)
    if offset != -1:
        log_info(f"Found WUP header at offset 0x{offset:X}")
    return offset


def find_rom_in_rpx(rpx_data, rom_data, system):
    """
    Try to find where the ROM is located in the RPX.
    Uses multiple strategies:
    1. Search for WUP header and check nearby
    2. Search for SNES header signatures
    3. Search for the first few KB of the ROM data
    """
    log_info("Searching for ROM location in RPX...")

    # Strategy 1: Find WUP header
    wup_offset = find_wup_header(rpx_data)
    if wup_offset != -1:
        # ROM typically follows the WUP header (after some metadata)
        # Common offsets after WUP header: 0x20, 0x30, 0x100, etc.
        for delta in [0x20, 0x30, 0x40, 0x100, 0x200]:
            candidate = wup_offset + delta
            if candidate + len(rom_data) <= len(rpx_data):
                # Check if this looks like ROM data
                if system == 'snes':
                    rom_type = detect_snes_rom_type(rpx_data[candidate:candidate + 0x10000])
                    if rom_type:
                        log_info(f"Found potential SNES {rom_type} ROM at offset 0x{candidate:X}")
                        return candidate

    # Strategy 2: Search for SNES header signatures in RPX
    if system == 'snes':
        # Get the expected header location from the ROM
        rom_type = detect_snes_rom_type(rom_data)
        if rom_type == 'lorom':
            header_offset = SNES_LOROM_HEADER
        elif rom_type == 'hirom':
            header_offset = SNES_HIROM_HEADER
        else:
            header_offset = SNES_LOROM_HEADER  # Default to LoROM

        # Get the original ROM's header bytes
        if len(rom_data) > header_offset + 0x20:
            # Search for a valid SNES header in the RPX
            # We look for areas that have valid header signatures
            search_start = 0x100000  # Start after typical code section
            search_end = len(rpx_data) - 0x10000

            for offset in range(search_start, search_end, 0x8000):
                if validate_snes_header(rpx_data, offset + header_offset):
                    log_info(f"Found SNES header signature at offset 0x{offset:X}")
                    return offset

    # Strategy 3: Search for first 1KB of ROM data
    search_bytes = rom_data[:1024]
    offset = rpx_data.find(search_bytes)
    if offset != -1:
        log_info(f"Found ROM data match at offset 0x{offset:X}")
        return offset

    return -1


def get_original_rom_size(rpx_data, wup_offset):
    """
    Try to extract the original ROM size from the WUP header.
    Size encoding at offset 0x0E from WUP:
        0x40 = 4MB, 0x20 = 2MB, 0x10 = 1MB, 0x08 = 512KB,
        0x04 = 256KB, 0x02 = 128KB, 0x01 = 64KB, 0x00 = 32KB
    """
    if wup_offset == -1 or wup_offset + 0x10 >= len(rpx_data):
        return None

    size_byte = rpx_data[wup_offset + 0x0E]

    size_map = {
        0x40: 4 * 1024 * 1024,
        0x20: 2 * 1024 * 1024,
        0x10: 1 * 1024 * 1024,
        0x08: 512 * 1024,
        0x04: 256 * 1024,
        0x02: 128 * 1024,
        0x01: 64 * 1024,
        0x00: 32 * 1024,
    }

    return size_map.get(size_byte)


def inject_rom(rpx_data, rom_data, offset, original_size=None):
    """
    Inject ROM data into RPX at the specified offset.
    Pads or truncates as needed.
    """
    rom_size = len(rom_data)

    if original_size is None:
        # Use ROM size rounded up to next power of 2
        original_size = 1
        while original_size < rom_size:
            original_size *= 2

    if rom_size > original_size:
        log_error(f"ROM ({rom_size} bytes) is larger than base game ROM ({original_size} bytes)")
        log_error("Injection would corrupt the RPX. Use a base game with a larger ROM.")
        return None

    if rom_size < original_size:
        # Pad ROM with 0xFF (common for cartridge ROMs)
        log_info(f"Padding ROM from {rom_size} to {original_size} bytes")
        rom_data = rom_data + bytearray([0xFF] * (original_size - rom_size))

    # Check bounds
    if offset + original_size > len(rpx_data):
        log_error(f"Offset 0x{offset:X} + ROM size exceeds RPX file size")
        return None

    # Inject
    log_info(f"Injecting {original_size} bytes at offset 0x{offset:X}")
    result = rpx_data[:offset] + rom_data + rpx_data[offset + original_size:]

    return result


def update_snes_header_checksum(data, offset, rom_type):
    """
    Recalculate and update the SNES internal checksum.
    """
    if rom_type == 'lorom':
        header_offset = offset + SNES_LOROM_HEADER
    else:
        header_offset = offset + SNES_HIROM_HEADER

    # Calculate checksum (sum of all bytes, truncated to 16 bits)
    checksum = sum(data[offset:]) & 0xFFFF
    complement = checksum ^ 0xFFFF

    # Update header
    struct.pack_into('<H', data, header_offset + SNES_CHECKSUM_COMP_OFFSET, complement)
    struct.pack_into('<H', data, header_offset + SNES_CHECKSUM_OFFSET, checksum)

    log_info(f"Updated SNES checksum: 0x{checksum:04X} (complement: 0x{complement:04X})")


def main():
    parser = argparse.ArgumentParser(
        description='Inject SNES/NES ROMs into Wii U VC RPX files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Auto-detect ROM offset
    python3 rpx_inject.py game.rpx myrom.sfc snes

    # Specify offset manually
    python3 rpx_inject.py game.rpx myrom.sfc snes --offset 0x788480

    # Output to different file
    python3 rpx_inject.py game.rpx myrom.sfc snes -o injected.rpx

Notes:
    - The RPX must be decompressed first using: wiiurpxtool -d file.rpx
    - After injection, recompress using: wiiurpxtool -c file.rpx
    - ROM must be same size or smaller than the base game's ROM
    - .smc headers are automatically stripped
"""
    )

    parser.add_argument('rpx_file', help='Decompressed RPX file')
    parser.add_argument('rom_file', help='ROM file to inject')
    parser.add_argument('system', choices=['snes', 'nes'], help='System type')
    parser.add_argument('--offset', '-O', type=lambda x: int(x, 0),
                        help='ROM offset in RPX (hex or decimal)')
    parser.add_argument('--output', '-o', help='Output file (default: overwrite input)')
    parser.add_argument('--rom-size', '-s', type=lambda x: int(x, 0),
                        help='Original ROM size (auto-detected if not specified)')
    parser.add_argument('--dry-run', '-n', action='store_true',
                        help='Show what would be done without modifying files')

    args = parser.parse_args()

    # Read files
    log_info(f"Reading RPX: {args.rpx_file}")
    rpx_data = read_file(args.rpx_file)
    log_info(f"RPX size: {len(rpx_data)} bytes")

    log_info(f"Reading ROM: {args.rom_file}")
    rom_data = read_file(args.rom_file)
    log_info(f"ROM size: {len(rom_data)} bytes")

    # Strip SMC header if present
    if args.system == 'snes':
        rom_data = strip_smc_header(rom_data)

        # Detect ROM type
        rom_type = detect_snes_rom_type(rom_data)
        if rom_type:
            log_info(f"Detected SNES {rom_type.upper()} ROM")
        else:
            log_warn("Could not detect SNES ROM type, assuming LoROM")
            rom_type = 'lorom'
    else:
        rom_type = None

    # Find or use provided offset
    if args.offset is not None:
        offset = args.offset
        log_info(f"Using provided offset: 0x{offset:X}")
    else:
        offset = find_rom_in_rpx(rpx_data, rom_data, args.system)
        if offset == -1:
            log_error("Could not auto-detect ROM offset in RPX")
            log_error("Please provide offset manually with --offset")
            log_info("")
            log_info("To find the offset manually:")
            log_info("  1. Open the decompressed RPX in a hex editor")
            log_info("  2. Search for 'WUP-' to find the WUP header")
            log_info("  3. The ROM typically starts 0x20-0x100 bytes after")
            log_info("  4. Or search for the ROM's first few bytes")
            sys.exit(1)

    # Get original ROM size
    original_size = args.rom_size
    if original_size is None:
        wup_offset = find_wup_header(rpx_data)
        original_size = get_original_rom_size(rpx_data, wup_offset)
        if original_size:
            log_info(f"Detected original ROM size: {original_size} bytes ({original_size // 1024} KB)")

    if args.dry_run:
        log_info("[DRY RUN] Would inject ROM at offset 0x{:X}".format(offset))
        sys.exit(0)

    # Perform injection
    result = inject_rom(rpx_data, rom_data, offset, original_size)
    if result is None:
        sys.exit(1)

    # Update checksum if SNES
    if args.system == 'snes' and rom_type:
        update_snes_header_checksum(result, offset, rom_type)

    # Write output
    output_file = args.output or args.rpx_file
    log_info(f"Writing result to: {output_file}")
    write_file(output_file, result)

    log_success("ROM injection complete!")
    log_info("")
    log_info("Next steps:")
    log_info(f"  1. Recompress RPX: wiiurpxtool -c {output_file}")
    log_info("  2. Package with NUSPacker")


if __name__ == '__main__':
    main()
