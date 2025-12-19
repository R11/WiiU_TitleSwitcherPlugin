// Async image loading with priority queue and caching

#pragma once

#include "renderer.h"
#include <cstdint>

namespace ImageLoader {

constexpr int DEFAULT_CACHE_SIZE = 500;
constexpr int ICON_WIDTH = 128;
constexpr int ICON_HEIGHT = 128;

enum class Priority {
    LOW,
    NORMAL,
    HIGH
};

enum class Status {
    NOT_REQUESTED,
    QUEUED,
    LOADING,
    READY,
    FAILED
};

// Lifecycle
bool Init(int cacheSize = DEFAULT_CACHE_SIZE);
void Shutdown();
void Update();

// Requests
void Request(uint64_t titleId, Priority priority = Priority::NORMAL);
void Cancel(uint64_t titleId);
void SetPriority(uint64_t titleId, Priority priority);

// Access
Status GetStatus(uint64_t titleId);
bool IsReady(uint64_t titleId);
Renderer::ImageHandle Get(uint64_t titleId);
void GetDebugInfo(int* outUpdateCalls, int* outQueueSize, bool* outInitialized);
void GetLoadingStats(int* outPending, int* outReady, int* outFailed, int* outTotal);

// Retry failed loads (call when menu opens)
void RetryFailed();

// Cache management
void ClearCache();
void Evict(uint64_t titleId);
int GetCacheCount();
int GetCacheCapacity();

// Batch operations
void Prefetch(const uint64_t* titleIds, int count);
void LoadAllSync();

} // namespace ImageLoader
