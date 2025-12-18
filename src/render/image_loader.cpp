/**
 * Image Loader Implementation
 * Loads title icons using libgd for image parsing.
 */

#include "image_loader.h"
#include "renderer.h"

#include <map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gd.h>
#include <nn/acp/title.h>

namespace ImageLoader {

namespace {

bool isInitialized = false;
int cacheCapacity = DEFAULT_CACHE_SIZE;

struct RequestInfo {
    uint64_t titleId;
    Priority priority;
    Status status;
    Renderer::ImageHandle handle;
};

std::map<uint64_t, RequestInfo> requestMap;
std::vector<uint64_t> loadQueue;
std::vector<uint64_t> cacheAccessOrder;

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

void touchCache(uint64_t titleId)
{
    auto cacheIterator = std::find(cacheAccessOrder.begin(), cacheAccessOrder.end(), titleId);
    if (cacheIterator != cacheAccessOrder.end()) {
        cacheAccessOrder.erase(cacheIterator);
    }
    cacheAccessOrder.push_back(titleId);
}

void freeImage(Renderer::ImageHandle handle)
{
    if (handle) {
        if (handle->pixels) {
            free(handle->pixels);
        }
        delete handle;
    }
}

void evictIfNeeded()
{
    while (static_cast<int>(cacheAccessOrder.size()) > cacheCapacity && !cacheAccessOrder.empty()) {
        uint64_t oldestTitleId = cacheAccessOrder.front();
        cacheAccessOrder.erase(cacheAccessOrder.begin());

        auto requestIterator = requestMap.find(oldestTitleId);
        if (requestIterator != requestMap.end() && requestIterator->second.status == Status::READY) {
            freeImage(requestIterator->second.handle);
            requestIterator->second.status = Status::NOT_REQUESTED;
            requestIterator->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

bool loadImageFromFile(const char* filePath, Renderer::ImageHandle& outputHandle)
{
    outputHandle = Renderer::INVALID_IMAGE;

    FILE* file = fopen(filePath, "rb");
    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 8) {
        fclose(file);
        return false;
    }

    uint8_t* fileBuffer = (uint8_t*)malloc(fileSize);
    if (!fileBuffer) {
        fclose(file);
        return false;
    }

    if (fread(fileBuffer, 1, fileSize, file) != (size_t)fileSize) {
        free(fileBuffer);
        fclose(file);
        return false;
    }
    fclose(file);

    gdImagePtr gdImage = nullptr;

    if (fileBuffer[0] == 0x89 && fileBuffer[1] == 'P' && fileBuffer[2] == 'N' && fileBuffer[3] == 'G') {
        gdImage = gdImageCreateFromPngPtr(fileSize, fileBuffer);
    } else if (fileBuffer[0] == 0xFF && fileBuffer[1] == 0xD8) {
        gdImage = gdImageCreateFromJpegPtr(fileSize, fileBuffer);
    } else if (fileBuffer[0] == 'B' && fileBuffer[1] == 'M') {
        gdImage = gdImageCreateFromBmpPtr(fileSize, fileBuffer);
    } else if (fileBuffer[0] == 0x00) {
        gdImage = gdImageCreateFromTgaPtr(fileSize, fileBuffer);
    }

    free(fileBuffer);

    if (!gdImage) {
        return false;
    }

    int imageWidth = gdImageSX(gdImage);
    int imageHeight = gdImageSY(gdImage);

    uint32_t* pixelBuffer = (uint32_t*)malloc(imageWidth * imageHeight * sizeof(uint32_t));
    if (!pixelBuffer) {
        gdImageDestroy(gdImage);
        return false;
    }

    for (int pixelY = 0; pixelY < imageHeight; pixelY++) {
        for (int pixelX = 0; pixelX < imageWidth; pixelX++) {
            int pixelValue = gdImageGetPixel(gdImage, pixelX, pixelY);

            uint8_t red = gdImageRed(gdImage, pixelValue);
            uint8_t green = gdImageGreen(gdImage, pixelValue);
            uint8_t blue = gdImageBlue(gdImage, pixelValue);
            uint8_t alpha = 255 - (gdImageAlpha(gdImage, pixelValue) * 2);

            pixelBuffer[pixelY * imageWidth + pixelX] = (red << 24) | (green << 16) | (blue << 8) | alpha;
        }
    }

    gdImageDestroy(gdImage);

    Renderer::ImageData* imageData = new Renderer::ImageData();
    imageData->pixels = pixelBuffer;
    imageData->width = imageWidth;
    imageData->height = imageHeight;

    outputHandle = imageData;
    return true;
}

bool loadImage(uint64_t titleId, Renderer::ImageHandle& outputHandle)
{
    outputHandle = Renderer::INVALID_IMAGE;

    if (!Renderer::SupportsImages()) {
        return false;
    }

    char metaDirectoryPath[256];
    ACPResult result = ACPGetTitleMetaDir(titleId, metaDirectoryPath, sizeof(metaDirectoryPath));
    if (result != ACP_RESULT_SUCCESS) {
        return false;
    }

    char iconFilePath[280];
    snprintf(iconFilePath, sizeof(iconFilePath), "%s/iconTex.tga", metaDirectoryPath);

    return loadImageFromFile(iconFilePath, outputHandle);
}

}

bool Init(int maxCacheSize)
{
    if (isInitialized) {
        return true;
    }

    cacheCapacity = (maxCacheSize > 0) ? maxCacheSize : DEFAULT_CACHE_SIZE;
    requestMap.clear();
    loadQueue.clear();
    cacheAccessOrder.clear();

    isInitialized = true;
    return true;
}

void Shutdown()
{
    if (!isInitialized) {
        return;
    }

    for (auto& entry : requestMap) {
        if (entry.second.handle) {
            freeImage(entry.second.handle);
            entry.second.handle = Renderer::INVALID_IMAGE;
        }
    }

    requestMap.clear();
    loadQueue.clear();
    cacheAccessOrder.clear();
    isInitialized = false;
}

void Update()
{
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
    if (loadImage(titleId, handle)) {
        requestIterator->second.status = Status::READY;
        requestIterator->second.handle = handle;
        touchCache(titleId);
        evictIfNeeded();
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
            touchCache(titleId);
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
        touchCache(titleId);
        return requestIterator->second.handle;
    }
    return Renderer::INVALID_IMAGE;
}

void ClearCache()
{
    if (!isInitialized) {
        return;
    }

    for (auto& entry : requestMap) {
        if (entry.second.handle) {
            freeImage(entry.second.handle);
        }
    }

    requestMap.clear();
    loadQueue.clear();
    cacheAccessOrder.clear();
}

void Evict(uint64_t titleId)
{
    if (!isInitialized) {
        return;
    }

    auto requestIterator = requestMap.find(titleId);
    if (requestIterator != requestMap.end()) {
        if (requestIterator->second.handle) {
            freeImage(requestIterator->second.handle);
        }
        requestMap.erase(requestIterator);
    }

    auto cacheIterator = std::find(cacheAccessOrder.begin(), cacheAccessOrder.end(), titleId);
    if (cacheIterator != cacheAccessOrder.end()) {
        cacheAccessOrder.erase(cacheIterator);
    }

    auto queueIterator = std::find(loadQueue.begin(), loadQueue.end(), titleId);
    if (queueIterator != loadQueue.end()) {
        loadQueue.erase(queueIterator);
    }
}

int GetCacheCount()
{
    return static_cast<int>(cacheAccessOrder.size());
}

int GetCacheCapacity()
{
    return cacheCapacity;
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

}
