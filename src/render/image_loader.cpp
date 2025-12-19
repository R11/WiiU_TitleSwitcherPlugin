/**
 * Async image loading with priority queue.
 * Uses ImageStore for loading from various sources.
 */

#include "image_loader.h"
#include "renderer.h"
#include "../storage/image_store.h"

#include <map>
#include <vector>
#include <algorithm>

namespace ImageLoader {

namespace {

bool isInitialized = false;

struct RequestInfo {
    uint64_t titleId;
    Priority priority;
    Status status;
    Renderer::ImageHandle handle;
};

std::map<uint64_t, RequestInfo> requestMap;
std::vector<uint64_t> loadQueue;

int updateCallCount = 0;
int lastQueueSize = 0;

void sortLoadQueue()
{
    std::sort(loadQueue.begin(), loadQueue.end(),
        [](uint64_t titleIdFirst, uint64_t titleIdSecond) {
            auto requestIterFirst = requestMap.find(titleIdFirst);
            auto requestIterSecond = requestMap.find(titleIdSecond);
            if (requestIterFirst == requestMap.end()) return false;
            if (requestIterSecond == requestMap.end()) return true;
            return static_cast<int>(requestIterFirst->second.priority) >
                   static_cast<int>(requestIterSecond->second.priority);
        });
}

}

bool Init(int cacheSize)
{
    if (isInitialized) {
        return true;
    }

    ImageStore::Init(cacheSize > 0 ? cacheSize : DEFAULT_CACHE_SIZE);

    requestMap.clear();
    loadQueue.clear();
    isInitialized = true;
    return true;
}

void Shutdown()
{
    if (!isInitialized) {
        return;
    }

    ImageStore::Shutdown();
    requestMap.clear();
    loadQueue.clear();
    isInitialized = false;
}

void Update()
{
    updateCallCount++;
    lastQueueSize = static_cast<int>(loadQueue.size());

    if (!isInitialized || loadQueue.empty()) {
        return;
    }

    sortLoadQueue();

    uint64_t titleId = loadQueue.front();
    loadQueue.erase(loadQueue.begin());

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator == requestMap.end()) {
        return;
    }

    requestIterator->second.status = Status::LOADING;

    Renderer::ImageHandle handle;
    if (ImageStore::Load(titleId, handle)) {
        requestIterator->second.status = Status::READY;
        requestIterator->second.handle = handle;
    } else {
        requestIterator->second.status = Status::FAILED;
        requestIterator->second.handle = Renderer::INVALID_IMAGE;
    }
}

void Request(uint64_t titleId, Priority priority)
{
    if (!isInitialized) {
        return;
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end()) {
        if (requestIterator->second.status == Status::READY) {
            return;
        }
        if (requestIterator->second.status == Status::QUEUED ||
            requestIterator->second.status == Status::LOADING) {
            if (priority > requestIterator->second.priority) {
                requestIterator->second.priority = priority;
            }
            return;
        }
    }

    RequestInfo newRequest;
    newRequest.titleId = titleId;
    newRequest.priority = priority;
    newRequest.status = Status::QUEUED;
    newRequest.handle = Renderer::INVALID_IMAGE;

    requestMap[titleId] = newRequest;
    loadQueue.push_back(titleId);
}

void Cancel(uint64_t titleId)
{
    if (!isInitialized) {
        return;
    }

    auto queueIterator = std::find(loadQueue.begin(), loadQueue.end(), titleId);
    if (queueIterator != loadQueue.end()) {
        loadQueue.erase(queueIterator);
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end() && requestIterator->second.status == Status::QUEUED) {
        requestIterator->second.status = Status::NOT_REQUESTED;
    }
}

void SetPriority(uint64_t titleId, Priority priority)
{
    if (!isInitialized) {
        return;
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end()) {
        requestIterator->second.priority = priority;
    }
}

Status GetStatus(uint64_t titleId)
{
    if (!isInitialized) {
        return Status::NOT_REQUESTED;
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end()) {
        return requestIterator->second.status;
    }
    return Status::NOT_REQUESTED;
}

bool IsReady(uint64_t titleId)
{
    return GetStatus(titleId) == Status::READY;
}

Renderer::ImageHandle Get(uint64_t titleId)
{
    if (!isInitialized) {
        return Renderer::INVALID_IMAGE;
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end() && requestIterator->second.status == Status::READY) {
        return requestIterator->second.handle;
    }
    return Renderer::INVALID_IMAGE;
}

void GetDebugInfo(int* outUpdateCalls, int* outQueueSize, bool* outInitialized)
{
    if (outUpdateCalls) *outUpdateCalls = updateCallCount;
    if (outQueueSize) *outQueueSize = lastQueueSize;
    if (outInitialized) *outInitialized = isInitialized;
}

void GetLoadingStats(int* outPending, int* outReady, int* outFailed, int* outTotal)
{
    int pending = 0, ready = 0, failed = 0;

    for (const auto& entry : requestMap) {
        switch (entry.second.status) {
            case Status::QUEUED:
            case Status::LOADING:
                pending++;
                break;
            case Status::READY:
                ready++;
                break;
            case Status::FAILED:
                failed++;
                break;
            default:
                break;
        }
    }

    if (outPending) *outPending = pending;
    if (outReady) *outReady = ready;
    if (outFailed) *outFailed = failed;
    if (outTotal) *outTotal = pending + ready + failed;
}

void RetryFailed()
{
    if (!isInitialized) {
        return;
    }

    for (auto& entry : requestMap) {
        if (entry.second.status == Status::FAILED) {
            entry.second.status = Status::QUEUED;
            loadQueue.push_back(entry.first);
        }
    }
}

void ClearCache()
{
    if (!isInitialized) {
        return;
    }

    ImageStore::ClearMemoryCache();
    requestMap.clear();
    loadQueue.clear();
}

void Evict(uint64_t titleId)
{
    if (!isInitialized) {
        return;
    }

    ImageStore::RemoveFromMemoryCache(titleId);

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end()) {
        requestMap.erase(requestIterator);
    }

    auto queueIterator = std::find(loadQueue.begin(), loadQueue.end(), titleId);
    if (queueIterator != loadQueue.end()) {
        loadQueue.erase(queueIterator);
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

void Prefetch(const uint64_t* titleIdArray, int count)
{
    if (!isInitialized || !titleIdArray) {
        return;
    }

    for (int index = 0; index < count; index++) {
        Request(titleIdArray[index], Priority::LOW);
    }
}

void LoadAllSync()
{
    if (!isInitialized) {
        return;
    }

    while (!loadQueue.empty()) {
        uint64_t titleId = loadQueue.front();
        loadQueue.erase(loadQueue.begin());

        auto requestIterator = requestMap.find(titleId);
        if (requestIterator == requestMap.end()) {
            continue;
        }

        Renderer::ImageHandle handle;
        if (ImageStore::Load(titleId, handle)) {
            requestIterator->second.status = Status::READY;
            requestIterator->second.handle = handle;
        } else {
            requestIterator->second.status = Status::FAILED;
            requestIterator->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

}
