/**
 * Mock ImageLoader for Web Preview
 *
 * Provides stub implementations - icons are not loaded in web preview.
 */

#include "render/image_loader.h"

namespace ImageLoader {

static bool sInitialized = false;

bool Init(int cacheSize) {
    (void)cacheSize;
    sInitialized = true;
    return true;
}

void Shutdown() {
    sInitialized = false;
}

void Update() {
    // No-op
}

void Request(uint64_t titleId, Priority priority) {
    (void)titleId;
    (void)priority;
}

void Cancel(uint64_t titleId) {
    (void)titleId;
}

void SetPriority(uint64_t titleId, Priority priority) {
    (void)titleId;
    (void)priority;
}

Status GetStatus(uint64_t titleId) {
    (void)titleId;
    return Status::NOT_REQUESTED;
}

bool IsReady(uint64_t titleId) {
    (void)titleId;
    return false;
}

bool HasHighPriorityPending() {
    return false;
}

Renderer::ImageHandle Get(uint64_t titleId) {
    (void)titleId;
    return Renderer::ImageHandle{};
}

void GetDebugInfo(int* outUpdateCalls, int* outQueueSize, bool* outInitialized) {
    if (outUpdateCalls) *outUpdateCalls = 0;
    if (outQueueSize) *outQueueSize = 0;
    if (outInitialized) *outInitialized = sInitialized;
}

void GetLoadingStats(int* outPending, int* outReady, int* outFailed, int* outTotal) {
    if (outPending) *outPending = 0;
    if (outReady) *outReady = 0;
    if (outFailed) *outFailed = 0;
    if (outTotal) *outTotal = 0;
}

void RetryFailed() {
    // No-op
}

void ClearCache() {
    // No-op
}

void Evict(uint64_t titleId) {
    (void)titleId;
}

int GetCacheCount() {
    return 0;
}

int GetCacheCapacity() {
    return 0;
}

void Prefetch(const uint64_t* titleIds, int count) {
    (void)titleIds;
    (void)count;
}

void LoadAllSync() {
    // No-op
}

} // namespace ImageLoader
