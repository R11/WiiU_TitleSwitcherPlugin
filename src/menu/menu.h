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

void OnApplicationStart();
void OnApplicationEnd();
void OnForegroundAcquired();
void OnForegroundReleased();

}
