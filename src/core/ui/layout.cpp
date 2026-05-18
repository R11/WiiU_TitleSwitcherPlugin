/**
 * Pixel-Based Layout System - Implementation
 */

#include "layout.h"

namespace Layout {

// =============================================================================
// Screen Resolution Data
// =============================================================================

static const ScreenInfo SCREEN_INFO[] = {
    { 854,  480,  "DRC (GamePad)", false },
    { 1920, 1080, "TV 1080p",      false },
    { 1280, 720,  "TV 720p",       false },
    { 640,  480,  "TV 480p (4:3)", true  },
};

const ScreenInfo& GetScreenInfo(ScreenType type) {
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(ScreenType::COUNT)) {
        return SCREEN_INFO[idx];
    }
    return SCREEN_INFO[0];
}

// =============================================================================
// Base Layout Values Per Resolution
// =============================================================================

struct BaseValues {
    int fontSize;
    int lineHeight;
    int charWidth;
    int iconSize;
    int margin;
    int headerHeight;
    int footerHeight;
    int categoryBarHeight;
    int panelGap;
};

static const BaseValues BASE_VALUES[] = {
    // DRC: 854x480 - close viewing, smaller text OK
    { 16, 24, 8, 128, 8, 24, 24, 24, 16 },

    // TV 1080p: 1920x1080 - far viewing, larger text
    { 24, 36, 12, 192, 16, 36, 36, 36, 24 },

    // TV 720p: 1280x720 - medium distance
    { 20, 30, 10, 160, 12, 30, 30, 30, 20 },

    // TV 480p: 640x480 - 4:3 aspect, compact
    { 16, 24, 8, 96, 8, 24, 24, 24, 12 },
};

// =============================================================================
// Divider Strings
// =============================================================================

static const char* HEADER_DIVIDER_60 = "------------------------------------------------------------";
static const char* HEADER_DIVIDER_80 = "--------------------------------------------------------------------------------";
static const char* SECTION_UNDERLINE = "--------";

// =============================================================================
// Layout Computation
// =============================================================================

PixelLayout ComputeLayout(ScreenType screen, const LayoutPreferences& prefs) {
    const ScreenInfo& info = GetScreenInfo(screen);
    int idx = static_cast<int>(screen);
    if (idx < 0 || idx >= static_cast<int>(ScreenType::COUNT)) {
        idx = 0;
    }
    const BaseValues& base = BASE_VALUES[idx];

    PixelLayout layout = {};

    // Screen dimensions
    layout.screenWidth = info.width;
    layout.screenHeight = info.height;

    // Font metrics (adjusted by fontScale preference)
    int scaledFontSize = (base.fontSize * prefs.fontScale) / 100;
    int scaledLineHeight = (base.lineHeight * prefs.fontScale) / 100;
    int scaledCharWidth = (base.charWidth * prefs.fontScale) / 100;

    // Ensure minimum sizes
    if (scaledFontSize < 8) scaledFontSize = 8;
    if (scaledLineHeight < 12) scaledLineHeight = 12;
    if (scaledCharWidth < 4) scaledCharWidth = 4;

    layout.font.size = scaledFontSize;
    layout.font.lineHeight = scaledLineHeight;
    layout.font.charWidth = scaledCharWidth;

    // Icon size (adjusted by iconSizePercent preference)
    layout.iconSize = (base.iconSize * prefs.iconSizePercent) / 100;
    if (layout.iconSize < 48) layout.iconSize = 48;

    // Chrome heights (scale with font)
    int categoryBarHeight = (base.categoryBarHeight * prefs.fontScale) / 100;
    int headerHeight = (base.headerHeight * prefs.fontScale) / 100;
    int footerHeight = (base.footerHeight * prefs.fontScale) / 100;

    // Category bar at top
    layout.chrome.categoryBar = {
        0,
        0,
        info.width,
        categoryBarHeight
    };

    // Header below category bar
    layout.chrome.header = {
        0,
        categoryBarHeight,
        info.width,
        headerHeight
    };

    // Footer at bottom
    layout.chrome.footer = {
        0,
        info.height - footerHeight,
        info.width,
        footerHeight
    };

    // Content area (between header and footer)
    int contentTop = categoryBarHeight + headerHeight;
    int contentBottom = info.height - footerHeight;
    int contentHeight = contentBottom - contentTop;

    // Panel widths (listWidthPercent determines left panel)
    int leftPanelWidth = (info.width * prefs.listWidthPercent) / 100;
    int panelGap = base.panelGap;
    int rightPanelX = leftPanelWidth + panelGap;
    int rightPanelWidth = info.width - rightPanelX - base.margin;

    // Left panel
    layout.leftPanel = {
        base.margin,
        leftPanelWidth - base.margin,
        contentTop,
        contentHeight,
        scaledLineHeight
    };

    // Right panel
    layout.rightPanel = {
        rightPanelX,
        rightPanelWidth,
        contentTop,
        contentHeight,
        scaledLineHeight
    };

    // Details section layout (within right panel)
    // Title area at top of right panel
    layout.details.titleArea = {
        rightPanelX,
        contentTop,
        rightPanelWidth,
        scaledLineHeight * 2
    };

    // Icon below title
    int iconY = contentTop + scaledLineHeight * 2 + base.margin;
    layout.details.icon = {
        rightPanelX + (rightPanelWidth - layout.iconSize) / 2,
        iconY,
        layout.iconSize,
        layout.iconSize
    };

    // Info area below icon
    int infoY = iconY + layout.iconSize + base.margin;
    int infoHeight = contentBottom - infoY;
    layout.details.infoArea = {
        rightPanelX,
        infoY,
        rightPanelWidth,
        infoHeight > 0 ? infoHeight : 0
    };

    // Divider strings based on panel width
    int dividerChars = rightPanelWidth / scaledCharWidth;
    if (dividerChars >= 80) {
        layout.dividers.header = HEADER_DIVIDER_80;
        layout.dividers.headerLength = 80;
    } else {
        layout.dividers.header = HEADER_DIVIDER_60;
        layout.dividers.headerLength = 60;
    }
    layout.dividers.sectionShort = SECTION_UNDERLINE;

    return layout;
}

// =============================================================================
// Cached Layout State
// =============================================================================

static ScreenType sCurrentScreenType = ScreenType::DRC;
static LayoutPreferences sCurrentPreferences = LayoutPreferences::Default();
static PixelLayout sCachedLayout = {};
static bool sLayoutValid = false;
static bool sInitialized = false;

ScreenType GetCurrentScreenType() {
    return sCurrentScreenType;
}

const LayoutPreferences& GetCurrentPreferences() {
    return sCurrentPreferences;
}

void SetCurrentPreferences(const LayoutPreferences& prefs) {
    sCurrentPreferences = prefs;
    sLayoutValid = false;
}

const PixelLayout& GetCurrentLayout() {
    if (!sLayoutValid) {
        sCachedLayout = ComputeLayout(sCurrentScreenType, sCurrentPreferences);
        sLayoutValid = true;
    }
    return sCachedLayout;
}

void InvalidateLayout() {
    sLayoutValid = false;
}

void Init() {
    if (sInitialized) return;

    sCurrentScreenType = ScreenType::DRC;
    sCurrentPreferences = LayoutPreferences::Default();
    sLayoutValid = false;
    sInitialized = true;
}

} // namespace Layout
