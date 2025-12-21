/**
 * Mock Layout implementation for unit tests
 *
 * Provides stub implementations of Layout functions used by settings.cpp.
 */

#include "../../src/ui/layout.h"

namespace Layout {

static LayoutPreferences sPrefs = LayoutPreferences::Default();
static PixelLayout sCachedLayout = {};

const ScreenInfo& GetScreenInfo(ScreenType type) {
    static ScreenInfo info = { 854, 480, "DRC", false };
    (void)type;
    return info;
}

PixelLayout ComputeLayout(ScreenType screen, const LayoutPreferences& prefs) {
    (void)screen;
    (void)prefs;
    return {};
}

ScreenType GetCurrentScreenType() {
    return ScreenType::DRC;
}

const LayoutPreferences& GetCurrentPreferences() {
    return sPrefs;
}

void SetCurrentPreferences(const LayoutPreferences& prefs) {
    sPrefs = prefs;
}

const PixelLayout& GetCurrentLayout() {
    return sCachedLayout;
}

void InvalidateLayout() {}

void Init() {}

} // namespace Layout
