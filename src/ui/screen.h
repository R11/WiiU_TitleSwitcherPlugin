/**
 * Screen Abstraction Layer
 *
 * Provides screen-agnostic rendering support for dual-screen layouts.
 * Allows the same rendering code to work on DRC or TV with appropriate scaling.
 *
 * SCREEN MODES:
 * - STANDARD: DRC shows grid, TV shows details
 * - SWAPPED: DRC shows details, TV shows grid
 * - MIRRORED: Both screens show grid
 * - DRC_ONLY: Only DRC is used (TV off)
 * - TV_ONLY: Only TV is used (DRC off)
 */

#pragma once

#include <cstdint>
#include "../common/screen_constants.h"

namespace Screen {

// Physical screen target
enum class Target {
    DRC,
    TV
};

// What content to display on a screen
enum class ContentMode {
    GRID,       // Icon grid for game selection
    DETAILS,    // Game information display
    OFF         // Screen not used
};

// User-configurable screen layout
struct Configuration {
    ContentMode drcMode;
    ContentMode tvMode;
};

// Preset screen configurations
namespace Presets {
    // Standard: Navigate on GamePad, details on TV
    constexpr Configuration STANDARD = {
        .drcMode = ContentMode::GRID,
        .tvMode = ContentMode::DETAILS
    };

    // Swapped: Navigate on TV, details on GamePad
    constexpr Configuration SWAPPED = {
        .drcMode = ContentMode::DETAILS,
        .tvMode = ContentMode::GRID
    };

    // Mirrored: Both screens show grid (for TV-only users)
    constexpr Configuration MIRRORED = {
        .drcMode = ContentMode::GRID,
        .tvMode = ContentMode::GRID
    };

    // GamePad only: TV is off
    constexpr Configuration DRC_ONLY = {
        .drcMode = ContentMode::GRID,
        .tvMode = ContentMode::OFF
    };

    // TV only: GamePad is off
    constexpr Configuration TV_ONLY = {
        .drcMode = ContentMode::OFF,
        .tvMode = ContentMode::GRID
    };
}

// Named preset for settings menu
enum class LayoutPreset {
    STANDARD,
    SWAPPED,
    MIRRORED,
    DRC_ONLY,
    TV_ONLY,

    COUNT
};

// Get configuration for a preset
inline Configuration GetPresetConfiguration(LayoutPreset preset) {
    switch (preset) {
        case LayoutPreset::STANDARD: return Presets::STANDARD;
        case LayoutPreset::SWAPPED:  return Presets::SWAPPED;
        case LayoutPreset::MIRRORED: return Presets::MIRRORED;
        case LayoutPreset::DRC_ONLY: return Presets::DRC_ONLY;
        case LayoutPreset::TV_ONLY:  return Presets::TV_ONLY;
        default: return Presets::STANDARD;
    }
}

// Get display label for a preset
inline const char* GetPresetLabel(LayoutPreset preset) {
    switch (preset) {
        case LayoutPreset::STANDARD: return "Standard (Pad: Grid, TV: Info)";
        case LayoutPreset::SWAPPED:  return "Swapped (Pad: Info, TV: Grid)";
        case LayoutPreset::MIRRORED: return "Mirrored (Both: Grid)";
        case LayoutPreset::DRC_ONLY: return "GamePad Only";
        case LayoutPreset::TV_ONLY:  return "TV Only";
        default: return "Unknown";
    }
}

/**
 * Screen Descriptor
 *
 * Runtime information about a screen, passed to render functions.
 * All layout calculations use this to adapt to the target screen.
 */
struct Descriptor {
    Target target;          // Which physical screen
    int width;              // Screen width in pixels
    int height;             // Screen height in pixels
    int marginX;            // Horizontal safe area margin
    int marginY;            // Vertical safe area margin
    float scale;            // UI scale factor (1.0 = DRC base)

    // Usable content area
    int contentWidth() const { return width - (marginX * 2); }
    int contentHeight() const { return height - (marginY * 2); }
    int contentX() const { return marginX; }
    int contentY() const { return marginY; }

    // Scale a base pixel value for this screen
    int scaled(int baseValue) const {
        return static_cast<int>(baseValue * scale);
    }

    // Scale a float value
    float scaledF(float baseValue) const {
        return baseValue * scale;
    }
};

// TV resolution detection
enum class TVResolution {
    P1080,
    P720,
    P480
};

// Get current TV resolution (stub - returns P720 as default)
TVResolution GetTVResolution();

// Build a screen descriptor for a target
Descriptor GetDescriptor(Target target);

// Build descriptor for DRC
inline Descriptor GetDRCDescriptor() {
    Descriptor desc;
    desc.target = Target::DRC;
    desc.width = DRC::WIDTH;
    desc.height = DRC::HEIGHT;
    desc.marginX = 27;
    desc.marginY = 24;
    desc.scale = 1.0f;
    return desc;
}

// Build descriptor for TV at current resolution
inline Descriptor GetTVDescriptor() {
    Descriptor desc;
    desc.target = Target::TV;

    switch (GetTVResolution()) {
        case TVResolution::P1080:
            desc.width = TV::P1080::WIDTH;
            desc.height = TV::P1080::HEIGHT;
            desc.marginX = 60;
            desc.marginY = 40;
            desc.scale = 2.0f;
            break;
        case TVResolution::P720:
            desc.width = TV::P720::WIDTH;
            desc.height = TV::P720::HEIGHT;
            desc.marginX = 40;
            desc.marginY = 30;
            desc.scale = 1.5f;
            break;
        case TVResolution::P480:
            desc.width = TV::P480::WIDTH;
            desc.height = TV::P480::HEIGHT;
            desc.marginX = 20;
            desc.marginY = 20;
            desc.scale = 1.0f;
            break;
    }

    return desc;
}

inline Descriptor GetDescriptor(Target target) {
    return (target == Target::DRC) ? GetDRCDescriptor() : GetTVDescriptor();
}

} // namespace Screen
