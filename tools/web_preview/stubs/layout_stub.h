/**
 * Layout Stub for Desktop Preview
 *
 * Provides the Layout API for preview tool.
 * Mirrors the main layout.h but runs on desktop.
 */

#pragma once

#include <cstdint>

namespace Layout {

// =============================================================================
// Screen Types
// =============================================================================

enum class ScreenType {
    DRC,        // GamePad: 854x480
    TV_1080P,   // TV 1080p: 1920x1080
    TV_720P,    // TV 720p: 1280x720
    TV_480P,    // TV 480p: 640x480 (4:3)

    COUNT
};

// =============================================================================
// Core Geometry Types
// =============================================================================

struct Rect {
    int x;
    int y;
    int width;
    int height;

    int Right() const { return x + width; }
    int Bottom() const { return y + height; }
    bool Contains(int px, int py) const {
        return px >= x && px < Right() && py >= y && py < Bottom();
    }
};

struct Panel {
    int x;
    int width;
    int contentY;
    int contentHeight;
    int rowHeight;

    int GetVisibleRows() const {
        return rowHeight > 0 ? contentHeight / rowHeight : 0;
    }

    int GetRowY(int row) const {
        return contentY + (row * rowHeight);
    }
};

// =============================================================================
// User Preferences
// =============================================================================

struct LayoutPreferences {
    int fontScale;          // 100 = default, range 75-150
    int listWidthPercent;   // 25-50, default 30
    int iconSizePercent;    // 50-150, default 100

    static LayoutPreferences Default() {
        return { 100, 30, 100 };
    }
};

// =============================================================================
// Pixel Layout
// =============================================================================

struct PixelLayout {
    int screenWidth;
    int screenHeight;

    struct FontMetrics {
        int size;
        int lineHeight;
        int charWidth;
    } font;

    struct Chrome {
        Rect categoryBar;
        Rect header;
        Rect footer;
    } chrome;

    Panel leftPanel;
    Panel rightPanel;

    struct Details {
        Rect icon;
        Rect titleArea;
        Rect infoArea;
    } details;

    int iconSize;

    struct Dividers {
        const char* header;
        const char* sectionShort;
        int headerLength;
    } dividers;

    int GetLeftPanelRowY(int row) const {
        return leftPanel.GetRowY(row);
    }

    int GetRightPanelRowY(int row) const {
        return rightPanel.GetRowY(row);
    }

    int GetLeftPanelMaxChars() const {
        return font.charWidth > 0 ? leftPanel.width / font.charWidth : 0;
    }

    int GetRightPanelMaxChars() const {
        return font.charWidth > 0 ? rightPanel.width / font.charWidth : 0;
    }
};

// =============================================================================
// Screen Resolution Info
// =============================================================================

struct ScreenInfo {
    int width;
    int height;
    const char* name;
    bool is4x3;
};

// =============================================================================
// Base Layout Values
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

inline const ScreenInfo SCREEN_INFO[] = {
    { 854,  480,  "DRC (GamePad)", false },
    { 1920, 1080, "TV 1080p",      false },
    { 1280, 720,  "TV 720p",       false },
    { 640,  480,  "TV 480p (4:3)", true  },
};

inline const BaseValues BASE_VALUES[] = {
    // DRC: 854x480 - close viewing, smaller text OK
    { 16, 24, 8, 128, 8, 24, 24, 24, 16 },
    // TV 1080p: 1920x1080 - far viewing, larger text
    { 24, 36, 12, 192, 16, 36, 36, 36, 24 },
    // TV 720p: 1280x720 - medium distance
    { 20, 30, 10, 160, 12, 30, 30, 30, 20 },
    // TV 480p: 640x480 - 4:3 aspect, compact
    { 16, 24, 8, 96, 8, 24, 24, 24, 12 },
};

inline const char* HEADER_DIVIDER_60 = "------------------------------------------------------------";
inline const char* HEADER_DIVIDER_80 = "--------------------------------------------------------------------------------";
inline const char* SECTION_UNDERLINE = "--------";

inline const ScreenInfo& GetScreenInfo(ScreenType type) {
    int idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(ScreenType::COUNT)) {
        return SCREEN_INFO[idx];
    }
    return SCREEN_INFO[0];
}

// =============================================================================
// Layout Computation
// =============================================================================

inline PixelLayout ComputeLayout(ScreenType screen, const LayoutPreferences& prefs) {
    const ScreenInfo& info = GetScreenInfo(screen);
    int idx = static_cast<int>(screen);
    if (idx < 0 || idx >= static_cast<int>(ScreenType::COUNT)) {
        idx = 0;
    }
    const BaseValues& base = BASE_VALUES[idx];

    PixelLayout layout = {};

    layout.screenWidth = info.width;
    layout.screenHeight = info.height;

    int scaledFontSize = (base.fontSize * prefs.fontScale) / 100;
    int scaledLineHeight = (base.lineHeight * prefs.fontScale) / 100;
    int scaledCharWidth = (base.charWidth * prefs.fontScale) / 100;

    if (scaledFontSize < 8) scaledFontSize = 8;
    if (scaledLineHeight < 12) scaledLineHeight = 12;
    if (scaledCharWidth < 4) scaledCharWidth = 4;

    layout.font.size = scaledFontSize;
    layout.font.lineHeight = scaledLineHeight;
    layout.font.charWidth = scaledCharWidth;

    layout.iconSize = (base.iconSize * prefs.iconSizePercent) / 100;
    if (layout.iconSize < 48) layout.iconSize = 48;

    int categoryBarHeight = (base.categoryBarHeight * prefs.fontScale) / 100;
    int headerHeight = (base.headerHeight * prefs.fontScale) / 100;
    int footerHeight = (base.footerHeight * prefs.fontScale) / 100;

    layout.chrome.categoryBar = { 0, 0, info.width, categoryBarHeight };
    layout.chrome.header = { 0, categoryBarHeight, info.width, headerHeight };
    layout.chrome.footer = { 0, info.height - footerHeight, info.width, footerHeight };

    int contentTop = categoryBarHeight + headerHeight;
    int contentBottom = info.height - footerHeight;
    int contentHeight = contentBottom - contentTop;

    int leftPanelWidth = (info.width * prefs.listWidthPercent) / 100;
    int panelGap = base.panelGap;
    int rightPanelX = leftPanelWidth + panelGap;
    int rightPanelWidth = info.width - rightPanelX - base.margin;

    layout.leftPanel = { base.margin, leftPanelWidth - base.margin, contentTop, contentHeight, scaledLineHeight };
    layout.rightPanel = { rightPanelX, rightPanelWidth, contentTop, contentHeight, scaledLineHeight };

    layout.details.titleArea = { rightPanelX, contentTop, rightPanelWidth, scaledLineHeight * 2 };

    int iconY = contentTop + scaledLineHeight * 2 + base.margin;
    layout.details.icon = { rightPanelX + (rightPanelWidth - layout.iconSize) / 2, iconY, layout.iconSize, layout.iconSize };

    int infoY = iconY + layout.iconSize + base.margin;
    int infoHeight = contentBottom - infoY;
    layout.details.infoArea = { rightPanelX, infoY, rightPanelWidth, infoHeight > 0 ? infoHeight : 0 };

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

inline ScreenType sCurrentScreenType = ScreenType::DRC;
inline LayoutPreferences sCurrentPreferences = LayoutPreferences::Default();
inline PixelLayout sCachedLayout = {};
inline bool sLayoutValid = false;
inline bool sInitialized = false;

inline ScreenType GetCurrentScreenType() {
    return sCurrentScreenType;
}

inline void SetCurrentScreenType(ScreenType type) {
    sCurrentScreenType = type;
    sLayoutValid = false;
}

inline const LayoutPreferences& GetCurrentPreferences() {
    return sCurrentPreferences;
}

inline void SetCurrentPreferences(const LayoutPreferences& prefs) {
    sCurrentPreferences = prefs;
    sLayoutValid = false;
}

inline const PixelLayout& GetCurrentLayout() {
    if (!sLayoutValid) {
        sCachedLayout = ComputeLayout(sCurrentScreenType, sCurrentPreferences);
        sLayoutValid = true;
    }
    return sCachedLayout;
}

inline void InvalidateLayout() {
    sLayoutValid = false;
}

inline void Init() {
    if (sInitialized) return;
    sCurrentScreenType = ScreenType::DRC;
    sCurrentPreferences = LayoutPreferences::Default();
    sLayoutValid = false;
    sInitialized = true;
}

} // namespace Layout
