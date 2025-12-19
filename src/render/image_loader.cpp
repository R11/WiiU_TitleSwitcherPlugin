// Async image loading with priority queue
// Uses ImageStore for actual loading from various sources

#include "image_loader.h"
#include "renderer.h"
#include "../storage/image_store.h"

#include <map>
#include <vector>
#include <algorithm>

namespace ImageLoader {

namespace {

bool sInitialized = false;

// Request tracking
struct RequestInfo {
    uint64_t titleId;
    Priority priority;
    Status status;
    Renderer::ImageHandle handle;
};

std::map<uint64_t, RequestInfo> sRequests;
std::vector<uint64_t> sLoadQueue;

// Debug stats
int sUpdateCallCount = 0;
int sQueueSize = 0;

void sortLoadQueue()
{
    std::sort(sLoadQueue.begin(), sLoadQueue.end(),
        [](uint64_t a, uint64_t b) {
            auto itA = sRequests.find(a);
            auto itB = sRequests.find(b);
            if (itA == sRequests.end()) return false;
            if (itB == sRequests.end()) return true;
            return static_cast<int>(itA->second.priority) >
                   static_cast<int>(itB->second.priority);
        });
}

} // anonymous namespace

bool Init(int cacheSize)
{
    if (sInitialized) {
        return true;
    }

    ImageStore::Init(cacheSize > 0 ? cacheSize : DEFAULT_CACHE_SIZE);

    sRequests.clear();
    sLoadQueue.clear();
    sInitialized = true;
    return true;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    ImageStore::Shutdown();
    sRequests.clear();
    sLoadQueue.clear();
    sInitialized = false;
}

void Update()
{
    sUpdateCallCount++;
    sQueueSize = static_cast<int>(sLoadQueue.size());

    if (!sInitialized || sLoadQueue.empty()) {
        return;
    }

    sortLoadQueue();

    uint64_t titleId = sLoadQueue.front();
    sLoadQueue.erase(sLoadQueue.begin());

    auto it = sRequests.find(titleId);
    if (it == sRequests.end()) {
        return;
    }

    it->second.status = Status::LOADING;

    Renderer::ImageHandle handle;
    if (ImageStore::Load(titleId, handle)) {
        it->second.status = Status::READY;
        it->second.handle = handle;
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
        if (it->second.status == Status::READY) {
            return;
        }
        if (it->second.status == Status::QUEUED ||
            it->second.status == Status::LOADING) {
            if (priority > it->second.priority) {
                it->second.priority = priority;
            }
            return;
        }
    }

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

    auto qIt = std::find(sLoadQueue.begin(), sLoadQueue.end(), titleId);
    if (qIt != sLoadQueue.end()) {
        sLoadQueue.erase(qIt);
    }

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
        return it->second.handle;
    }
    return Renderer::INVALID_IMAGE;
}

void GetDebugInfo(int* outUpdateCalls, int* outQueueSize, bool* outInitialized)
{
    if (outUpdateCalls) *outUpdateCalls = sUpdateCallCount;
    if (outQueueSize) *outQueueSize = sQueueSize;
    if (outInitialized) *outInitialized = sInitialized;
}

void ClearCache()
{
    if (!sInitialized) {
        return;
    }

    ImageStore::ClearMemoryCache();
    sRequests.clear();
    sLoadQueue.clear();
}

void Evict(uint64_t titleId)
{
    if (!sInitialized) {
        return;
    }

    ImageStore::RemoveFromMemoryCache(titleId);

    auto it = sRequests.find(titleId);
    if (it != sRequests.end()) {
        sRequests.erase(it);
    }

    auto queueIt = std::find(sLoadQueue.begin(), sLoadQueue.end(), titleId);
    if (queueIt != sLoadQueue.end()) {
        sLoadQueue.erase(queueIt);
    }
}

int GetCacheCount()
{
    return ImageStore::GetMemoryCacheCount();
}

int GetCacheCapacity()
{
    return ImageStore::GetMemoryCacheCapacity();
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

void LoadAllSync()
{
    if (!sInitialized) {
        return;
    }

    while (!sLoadQueue.empty()) {
        uint64_t titleId = sLoadQueue.front();
        sLoadQueue.erase(sLoadQueue.begin());

        auto it = sRequests.find(titleId);
        if (it == sRequests.end()) {
            continue;
        }

        Renderer::ImageHandle handle;
        if (ImageStore::Load(titleId, handle)) {
            it->second.status = Status::READY;
            it->second.handle = handle;
        } else {
            it->second.status = Status::FAILED;
            it->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

} // namespace ImageLoader
