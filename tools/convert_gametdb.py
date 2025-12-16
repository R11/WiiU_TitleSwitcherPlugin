#!/usr/bin/env python3
"""
GameTDB to TitleSwitcher Presets Converter

This script converts wiiutdb.xml from GameTDB to the JSON format used by
the TitleSwitcher plugin's presets system.

Usage:
    python3 convert_gametdb.py wiiutdb.xml TitleSwitcher_presets.json

Download wiiutdb.xml from: https://www.gametdb.com/wiiutdb.zip

The output JSON can be placed at:
    sd:/wiiu/plugins/config/TitleSwitcher_presets.json
"""

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from typing import Optional


def get_region_from_id(game_id: str) -> str:
    """
    Extract region from game ID.

    Wii U game IDs follow the format: XXXRPP where:
    - XXX = Title code (3 chars)
    - R = Region (1 char)
    - PP = Publisher code (2 chars)

    Region codes:
    - E = USA
    - P = Europe
    - J = Japan
    - K = Korea
    """
    if len(game_id) >= 4:
        region_char = game_id[3].upper()
        region_map = {
            'E': 'USA',
            'P': 'EUR',
            'J': 'JPN',
            'K': 'KOR',
            'W': 'TWN',
            'A': 'ALL',
        }
        return region_map.get(region_char, region_char)
    return ''


def parse_date(date_elem) -> Optional[str]:
    """Parse date element with year/month/day attributes."""
    if date_elem is None:
        return None

    year = date_elem.get('year')
    month = date_elem.get('month')
    day = date_elem.get('day')

    if not year:
        return None

    if month and day:
        return f"{year}-{month.zfill(2)}-{day.zfill(2)}"
    elif month:
        return f"{year}-{month.zfill(2)}"
    else:
        return year


def get_title(game_elem, lang='EN') -> str:
    """Get localized title, preferring specified language."""
    # Try specified language first
    for locale in game_elem.findall('locale'):
        if locale.get('lang', '').upper() == lang:
            title = locale.find('title')
            if title is not None and title.text:
                return title.text.strip()

    # Fallback to any locale
    for locale in game_elem.findall('locale'):
        title = locale.find('title')
        if title is not None and title.text:
            return title.text.strip()

    # Use name attribute as last resort
    return game_elem.get('name', '')


def get_genre(game_elem) -> str:
    """Extract primary genre from game element."""
    genre = game_elem.find('genre')
    if genre is not None and genre.text:
        # GameTDB uses comma-separated genres, take the first one
        genres = genre.text.split(',')
        if genres:
            return genres[0].strip()
    return ''


def convert_gametdb(input_file: str, output_file: str,
                    include_all_regions: bool = True,
                    preferred_lang: str = 'EN') -> int:
    """
    Convert wiiutdb.xml to TitleSwitcher presets JSON.

    Args:
        input_file: Path to wiiutdb.xml
        output_file: Path to output JSON file
        include_all_regions: If True, include all regional variants
        preferred_lang: Preferred language for titles (EN, JA, DE, FR, etc.)

    Returns:
        Number of titles converted
    """
    print(f"Parsing {input_file}...")

    try:
        tree = ET.parse(input_file)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"Error parsing XML: {e}", file=sys.stderr)
        return 0
    except FileNotFoundError:
        print(f"File not found: {input_file}", file=sys.stderr)
        return 0

    titles = []
    seen_ids = set()

    for game in root.findall('.//game'):
        game_id = game.find('id')
        if game_id is None or not game_id.text:
            continue

        gid = game_id.text.strip()

        # Skip duplicates
        if gid in seen_ids:
            continue
        seen_ids.add(gid)

        # Get title name
        name = get_title(game, preferred_lang)
        if not name:
            name = game.get('name', gid)

        # Build title entry
        entry = {
            'id': gid,
            'name': name
        }

        # Publisher
        publisher = game.find('publisher')
        if publisher is not None and publisher.text:
            entry['publisher'] = publisher.text.strip()

        # Developer
        developer = game.find('developer')
        if developer is not None and developer.text:
            entry['developer'] = developer.text.strip()

        # Release date
        date = parse_date(game.find('date'))
        if date:
            entry['releaseDate'] = date

        # Region (from game ID)
        region = get_region_from_id(gid)
        if region:
            entry['region'] = region

        # Genre
        genre = get_genre(game)
        if genre:
            entry['genre'] = genre

        titles.append(entry)

    # Sort by name for easier browsing
    titles.sort(key=lambda x: x['name'].lower())

    # Build output structure
    output = {
        'version': 1,
        'titles': titles
    }

    # Write JSON
    print(f"Writing {len(titles)} titles to {output_file}...")

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2, ensure_ascii=False)

    # Print statistics
    publishers = set(t.get('publisher', '') for t in titles if t.get('publisher'))
    developers = set(t.get('developer', '') for t in titles if t.get('developer'))
    genres = set(t.get('genre', '') for t in titles if t.get('genre'))
    regions = set(t.get('region', '') for t in titles if t.get('region'))
    with_date = sum(1 for t in titles if t.get('releaseDate'))

    print(f"\nStatistics:")
    print(f"  Total titles: {len(titles)}")
    print(f"  Unique publishers: {len(publishers)}")
    print(f"  Unique developers: {len(developers)}")
    print(f"  Unique genres: {len(genres)}")
    print(f"  Unique regions: {len(regions)}")
    print(f"  Titles with release date: {with_date}")

    return len(titles)


def main():
    parser = argparse.ArgumentParser(
        description='Convert GameTDB wiiutdb.xml to TitleSwitcher presets JSON',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
    %(prog)s wiiutdb.xml TitleSwitcher_presets.json
    %(prog)s --lang JA wiiutdb.xml presets_ja.json

Download wiiutdb.xml from: https://www.gametdb.com/wiiutdb.zip
        '''
    )

    parser.add_argument('input', help='Input wiiutdb.xml file')
    parser.add_argument('output', help='Output JSON file')
    parser.add_argument('--lang', default='EN',
                       help='Preferred language for titles (default: EN)')

    args = parser.parse_args()

    count = convert_gametdb(args.input, args.output, preferred_lang=args.lang)

    if count > 0:
        print(f"\nDone! Copy {args.output} to:")
        print("  sd:/wiiu/plugins/config/TitleSwitcher_presets.json")
        return 0
    else:
        return 1


if __name__ == '__main__':
    sys.exit(main())
