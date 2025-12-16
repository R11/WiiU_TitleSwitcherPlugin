/**
 * Image Loader Implementation
 *
 * This is currently a stub implementation. Full async loading, caching,
 * and priority queue will be implemented when the GX2 renderer is ready.
 *
 * For now, all requests return Status::FAILED since OSScreen cannot
 * display images.
 */

#include "image_loader.h"
#include "renderer.h"

#include <map>
#include <vector>
#include <algorithm>

namespace ImageLoader {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Is the loader initialized?
bool sInitialized = false;

// Maximum cache size
int sCacheCapacity = DEFAULT_CACHE_SIZE;

// Request tracking
struct RequestInfo {
    uint64_t titleId;
    Priority priority;
    Status status;
    Renderer::ImageHandle handle;
};

// Map of title ID -> request info
std::map<uint64_t, RequestInfo> sRequests;

// Priority queue for pending loads (sorted by priority)
std::vector<uint64_t> sLoadQueue;

// Cache tracking (LRU order - most recently used at end)
std::vector<uint64_t> sCacheOrder;

/**
 * Sort the load queue by priority (HIGH first).
 */
void sortLoadQueue()
{
    std::sort(sLoadQueue.begin(), sLoadQueue.end(),
        [](uint64_t a, uint64_t b) {
            auto itA = sRequests.find(a);
            auto itB = sRequests.find(b);
            if (itA == sRequests.end()) return false;
            if (itB == sRequests.end()) return true;
            // Higher priority value = higher priority
            return static_cast<int>(itA->second.priority) >
                   static_cast<int>(itB->second.priority);
        });
}

/**
 * Mark a cache entry as recently used.
 */
void touchCache(uint64_t titleId)
{
    auto it = std::find(sCacheOrder.begin(), sCacheOrder.end(), titleId);
    if (it != sCacheOrder.end()) {
        sCacheOrder.erase(it);
    }
    sCacheOrder.push_back(titleId);
}

/**
 * Evict the least recently used cache entry if at capacity.
 */
void evictIfNeeded()
{
    while (static_cast<int>(sCacheOrder.size()) > sCacheCapacity && !sCacheOrder.empty()) {
        uint64_t oldest = sCacheOrder.front();
        sCacheOrder.erase(sCacheOrder.begin());

        auto it = sRequests.find(oldest);
        if (it != sRequests.end() && it->second.status == Status::READY) {
            // TODO: Free the image handle when GX2 is implemented
            // For now, just mark as not requested
            it->second.status = Status::NOT_REQUESTED;
            it->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

/**
 * Actually load an image (placeholder for future implementation).
 */
bool loadImage(uint64_t titleId, Renderer::ImageHandle& outHandle)
{
    (void)titleId;
    outHandle = Renderer::INVALID_IMAGE;

    // Cannot load images if renderer doesn't support them
    if (!Renderer::SupportsImages()) {
        return false;
    }

    // TODO: Implement actual loading when GX2 is ready:
    // 1. Get meta directory path using ACPGetTitleMetaDir()
    // 2. Construct path to iconTex.tga
    // 3. Load and parse TGA file
    // 4. Convert to GX2 texture format
    // 5. Return handle

    return false;
}

} // anonymous namespace

// =============================================================================
// Public API Implementation
// =============================================================================

bool Init(int cacheSize)
{
    if (sInitialized) {
        return true;
    }

    sCacheCapacity = (cacheSize > 0) ? cacheSize : DEFAULT_CACHE_SIZE;
    sRequests.clear();
    sLoadQueue.clear();
    sCacheOrder.clear();

    sInitialized = true;
    return true;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    // TODO: Free all image handles when GX2 is implemented

    sRequests.clear();
    sLoadQueue.clear();
    sCacheOrder.clear();
    sInitialized = false;
}

void Update()
{
    if (!sInitialized || sLoadQueue.empty()) {
        return;
    }

    // Process one item from the queue per frame
    sortLoadQueue();

    uint64_t titleId = sLoadQueue.front();
    sLoadQueue.erase(sLoadQueue.begin());

    auto it = sRequests.find(titleId);
    if (it == sRequests.end()) {
        return;
    }

    it->second.status = Status::LOADING;

    Renderer::ImageHandle handle;
    if (loadImage(titleId, handle)) {
        it->second.status = Status::READY;
        it->second.handle = handle;
        touchCache(titleId);
        evictIfNeeded();
    } else {
        it->second.status = Status::FAILED;
        it->second.handle = Renderer::INVALID_IMAGE;
    }
}

void Request(uint64_t titleId, Priority priority)
{
    if (!sInitialized) {
        return;
    }

    auto it = sRequests.find(titleId);
    if (it != sRequests.end()) {
        // Already requested
        if (it->second.status == Status::READY) {
            // Already loaded, just touch the cache
            touchCache(titleId);
            return;
        }
        if (it->second.status == Status::QUEUED ||
            it->second.status == Status::LOADING) {
            // Update priority if higher
            if (priority > it->second.priority) {
                it->second.priority = priority;
            }
            return;
        }
    }

    // Create new request
    RequestInfo info;
    info.titleId = titleId;
    info.priority = priority;
    info.status = Status::QUEUED;
    info.handle = Renderer::INVALID_IMAGE;

    sRequests[titleId] = info;
    sLoadQueue.push_back(titleId);
}

void Cancel(uint64_t titleId)
{
    if (!sInitialized) {
        return;
    }

    // Remove from queue
    auto qIt = std::find(sLoadQueue.begin(), sLoadQueue.end(), titleId);
    if (qIt != sLoadQueue.end()) {
        sLoadQueue.erase(qIt);
    }

    // Update status (keep in map for cache purposes)
    auto it = sRequests.find(titleId);
    if (it != sRequests.end() && it->second.status == Status::QUEUED) {
        it->second.status = Status::NOT_REQUESTED;
    }
}

void SetPriority(uint64_t titleId, Priority priority)
{
    if (!sInitialized) {
        return;
    }

    auto it = sRequests.find(titleId);
    if (it != sRequests.end()) {
        it->second.priority = priority;
    }
}

Status GetStatus(uint64_t titleId)
{
    if (!sInitialized) {
        return Status::NOT_REQUESTED;
    }

    auto it = sRequests.find(titleId);
    if (it != sRequests.end()) {
        return it->second.status;
    }
    return Status::NOT_REQUESTED;
}

bool IsReady(uint64_t titleId)
{
    return GetStatus(titleId) == Status::READY;
}

Renderer::ImageHandle Get(uint64_t titleId)
{
    if (!sInitialized) {
        return Renderer::INVALID_IMAGE;
    }

    auto it = sRequests.find(titleId);
    if (it != sRequests.end() && it->second.status == Status::READY) {
        touchCache(titleId);
        return it->second.handle;
    }
    return Renderer::INVALID_IMAGE;
}

void ClearCache()
{
    if (!sInitialized) {
        return;
    }

    // TODO: Free all image handles when GX2 is implemented

    sRequests.clear();
    sLoadQueue.clear();
    sCacheOrder.clear();
}

void Evict(uint64_t titleId)
{
    if (!sInitialized) {
        return;
    }

    auto it = sRequests.find(titleId);
    if (it != sRequests.end()) {
        // TODO: Free image handle when GX2 is implemented
        sRequests.erase(it);
    }

    auto cacheIt = std::find(sCacheOrder.begin(), sCacheOrder.end(), titleId);
    if (cacheIt != sCacheOrder.end()) {
        sCacheOrder.erase(cacheIt);
    }

    auto queueIt = std::find(sLoadQueue.begin(), sLoadQueue.end(), titleId);
    if (queueIt != sLoadQueue.end()) {
        sLoadQueue.erase(queueIt);
    }
}

int GetCacheCount()
{
    return static_cast<int>(sCacheOrder.size());
}

int GetCacheCapacity()
{
    return sCacheCapacity;
}

void Prefetch(const uint64_t* titleIds, int count)
{
    if (!sInitialized || !titleIds) {
        return;
    }

    for (int i = 0; i < count; i++) {
        Request(titleIds[i], Priority::LOW);
    }
}

} // namespace ImageLoader
