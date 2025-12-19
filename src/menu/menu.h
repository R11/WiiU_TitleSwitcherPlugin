/**
 * Main Menu System
 * Handles menu state, rendering loop, input, and title launching.
 */

#pragma once

#include <cstdint>

namespace Menu {

enum class Mode {
    BROWSE,
    EDIT,
    SETTINGS,
    DEBUG_GRID
};

/**
 * Result of processing a single frame.
 * Used by ProcessFrame() to communicate state to caller.
 */
struct FrameResult {
    bool shouldContinue;     // false = menu closing
    uint64_t titleToLaunch;  // non-zero = launch this title after close
};

constexpr int CATEGORY_ROW = 0;
constexpr int HEADER_ROW = 1;
constexpr int LIST_START_ROW = 2;
constexpr int LIST_START_COL = 0;

void Init();
void Shutdown();

bool IsOpen();
bool IsSafeToOpen();
Mode GetMode();

void Open();
void Close();

/**
 * Process a single frame (render + input).
 * Non-blocking - returns immediately after one frame.
 * Used by web preview which needs callback-based main loop.
 */
FrameResult ProcessFrame();

/**
 * Render one frame without reading input.
 * Used by web preview for dual-screen rendering.
 */
void RenderFrame();

/**
 * Process input only (no rendering).
 * Returns FrameResult indicating if menu should continue.
 */
FrameResult HandleInputFrame();

/**
 * Reset menu to browse mode without closing.
 * Used by web preview after handling launch/cancel.
 */
void ResetToBrowse();

/**
 * Initialize menu state for web preview.
 * Sets sIsOpen=true without calling blocking runMenuLoop().
 */
void InitForWebPreview();

void OnApplicationStart();
void OnApplicationEnd();
void OnForegroundAcquired();
void OnForegroundReleased();

}
