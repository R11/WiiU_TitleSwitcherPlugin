/**
 * ImageLoader Stub for Desktop Preview
 *
 * Provides stub image loading - returns placeholder data.
 */

#pragma once

#include <cstdint>

// Forward declare - Renderer::ImageHandle is defined in renderer_stub.h
// We avoid including it here to prevent circular dependencies
namespace Renderer {
    struct ImageData;
    using ImageHandle = ImageData*;
}

namespace ImageLoader {

enum class Priority {
    LOW,
    NORMAL,
    HIGH
};

inline void Init() {}
inline void Shutdown() {}
inline void Update() {}

inline void Request(uint64_t titleId, Priority priority = Priority::NORMAL) {
    (void)titleId;
    (void)priority;
}

inline void SetPriority(uint64_t titleId, Priority priority) {
    (void)titleId;
    (void)priority;
}

inline bool IsReady(uint64_t titleId) {
    (void)titleId;
    return false;  // Never ready - always show placeholder
}

inline Renderer::ImageHandle Get(uint64_t titleId) {
    (void)titleId;
    return nullptr;
}

inline void ClearCache() {}

} // namespace ImageLoader
