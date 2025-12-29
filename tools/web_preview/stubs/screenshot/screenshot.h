/**
 * Screenshot Stub for Web Preview
 *
 * Provides no-op implementations of screenshot functions for the web preview.
 * Screenshots are not available in the browser environment.
 */

#pragma once

#include "../renderer_stub.h"

namespace Screenshot {

inline bool CaptureFromGame() { return false; }

inline Renderer::ImageHandle GetTVImage() { return nullptr; }

inline Renderer::ImageHandle GetDRCImage() { return nullptr; }

inline bool HasCapture() { return false; }

inline int GetCaptureWidth() { return 0; }

inline int GetCaptureHeight() { return 0; }

inline void Release() {}

}
