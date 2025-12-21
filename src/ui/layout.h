/**
 * Pixel-Based Layout System
 *
 * Defines screen layouts in pixel coordinates, enabling:
 * - Independent layouts per resolution (DRC, TV 1080p/720p/480p)
 * - Different content/arrangements for GamePad vs TV
 * - User-customizable font scale, list width, icon size
 * - Pixel-perfect rendering (font heights divide evenly into resolution)
 *
 * USAGE:
 * ------
 *   // Get layout for current screen with default preferences
 *   const auto& layout = Layout::GetCurrentLayout();
 *
 *   // Draw title list using layout coordinates
 *   for (int i = 0; i < layout.leftPanel.GetVisibleRows(); i++) {
 *       int y = layout.leftPanel.contentY + (i * layout.font.lineHeight);
 *       DrawText(layout.leftPanel.x, y, text, color);
 *   }
 *
 *   // Draw icon at layout-specified position
 *   DrawImage(layout.details.icon.x, layout.details.icon.y,
 *             icon, layout.iconSize, layout.iconSize);
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
    // Screen dimensions
    int screenWidth;
    int screenHeight;

    // Font metrics (resolution-specific, adjusted by fontScale)
    struct FontMetrics {
        int size;           // Font height in pixels
        int lineHeight;     // size + leading (row height for text)
        int charWidth;      // Approximate character width for monospace
    } font;

    // Chrome (header/footer areas)
    struct Chrome {
        Rect categoryBar;   // Top category tabs
        Rect header;        // Header/divider below categories
        Rect footer;        // Bottom status bar
    } chrome;

    // Two-panel layout
    Panel leftPanel;
    Panel rightPanel;

    // Browse mode details section (right panel)
    struct Details {
        Rect icon;          // Icon position (uses iconSize for dimensions)
        Rect titleArea;     // Title name text area
        Rect infoArea;      // Publisher, developer, etc.
    } details;

    // Icon size (separate field for clarity, used by details.icon)
    int iconSize;

    // Divider decorations
    struct Dividers {
        const char* header;         // e.g., "--------------------"
        const char* sectionShort;   // e.g., "--------"
        int headerLength;
    } dividers;

    // Helper: Get row Y position in left panel
    int GetLeftPanelRowY(int row) const {
        return leftPanel.GetRowY(row);
    }

    // Helper: Get row Y position in right panel
    int GetRightPanelRowY(int row) const {
        return rightPanel.GetRowY(row);
    }

    // Helper: Get maximum characters that fit in left panel
    int GetLeftPanelMaxChars() const {
        return font.charWidth > 0 ? leftPanel.width / font.charWidth : 0;
    }

    // Helper: Get maximum characters that fit in right panel
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

const ScreenInfo& GetScreenInfo(ScreenType type);

// =============================================================================
// Layout Access
// =============================================================================

/**
 * Compute layout for a screen type with given preferences.
 * This is the core function that builds a PixelLayout.
 */
PixelLayout ComputeLayout(ScreenType screen, const LayoutPreferences& prefs);

/**
 * Get the current screen type.
 * Currently returns DRC; future: detect actual output.
 */
ScreenType GetCurrentScreenType();

/**
 * Get the current preferences from settings.
 * Returns default preferences if settings not loaded.
 */
const LayoutPreferences& GetCurrentPreferences();

/**
 * Set the current preferences (updates cached layout).
 */
void SetCurrentPreferences(const LayoutPreferences& prefs);

/**
 * Get cached layout for current screen type and preferences.
 * Recomputes if preferences have changed.
 */
const PixelLayout& GetCurrentLayout();

/**
 * Force recomputation of cached layout.
 * Call after changing screen type or preferences.
 */
void InvalidateLayout();

// =============================================================================
// Initialization
// =============================================================================

/**
 * Initialize layout system.
 * Call once at startup.
 */
void Init();

} // namespace Layout
