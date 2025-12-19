/**
 * Image Loader Implementation
 *
 * Loads title icons using libgd for image parsing.
 * Icons are loaded from each title's meta directory (iconTex.tga).
 */

#include "image_loader.h"
#include "renderer.h"

#include <map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// libgd for image loading
#include <gd.h>

// Wii U SDK
#include <nn/acp/title.h>
#include <coreinit/title.h>

namespace ImageLoader {

// =============================================================================
// Internal State
// =============================================================================

namespace {

// Is the loader initialized?
bool sInitialized = false;

// Maximum cache size
int sCacheCapacity = DEFAULT_CACHE_SIZE;


// Debug: track Update() calls
int sUpdateCallCount = 0;
int sQueueSize = 0;

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
 * Free an image and its associated memory.
 */
void freeImage(Renderer::ImageHandle handle)
{
    if (handle) {
        if (handle->pixels) {
            free(handle->pixels);
        }
        delete handle;
    }
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
            // Free the image memory
            freeImage(it->second.handle);
            it->second.status = Status::NOT_REQUESTED;
            it->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

/**
 * Load an image from file into RGBA pixel data.
 */
bool loadImageFromFile(const char* path, Renderer::ImageHandle& outHandle)
{
    outHandle = Renderer::INVALID_IMAGE;

    FILE* file = fopen(path, "rb");
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

    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        fclose(file);
        return false;
    }

    if (fread(buffer, 1, fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);

    uint8_t hdr[4] = { buffer[0], buffer[1], buffer[2], buffer[3] };

    gdImagePtr gdImg = nullptr;
    if (hdr[0] == 0x89 && hdr[1] == 'P' && hdr[2] == 'N' && hdr[3] == 'G') {
        gdImg = gdImageCreateFromPngPtr(fileSize, buffer);
    } else if (hdr[0] == 0xFF && hdr[1] == 0xD8) {
        gdImg = gdImageCreateFromJpegPtr(fileSize, buffer);
    } else if (hdr[0] == 'B' && hdr[1] == 'M') {
        gdImg = gdImageCreateFromBmpPtr(fileSize, buffer);
    } else {
        gdImg = gdImageCreateFromTgaPtr(fileSize, buffer);
    }

    free(buffer);

    if (!gdImg) {
        return false;
    }

    // Get dimensions
    int width = gdImageSX(gdImg);
    int height = gdImageSY(gdImg);

    // Allocate pixel buffer
    uint32_t* pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (!pixels) {
        gdImageDestroy(gdImg);
        return false;
    }

    // Convert to RGBA
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel = gdImageGetPixel(gdImg, x, y);

            uint8_t r = gdImageRed(gdImg, pixel);
            uint8_t g = gdImageGreen(gdImg, pixel);
            uint8_t b = gdImageBlue(gdImg, pixel);
            // libgd alpha is 0-127 where 0=opaque, 127=transparent
            // Convert to 0-255 where 255=opaque
            uint8_t a = 255 - (gdImageAlpha(gdImg, pixel) * 2);

            // Store as RGBA (0xRRGGBBAA)
            pixels[y * width + x] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }

    gdImageDestroy(gdImg);

    // Create ImageData
    Renderer::ImageData* imageData = new Renderer::ImageData();
    imageData->pixels = pixels;
    imageData->width = width;
    imageData->height = height;

    outHandle = imageData;
    return true;
}

constexpr const char* ConfigDirectory = "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher";
constexpr const char* IconsDirectory = "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher/icons";

void copyFileToSDCard(const char* srcPath, uint64_t titleId)
{
    FILE* src = fopen(srcPath, "rb");
    if (!src) return;

    char dstPath[160];
    snprintf(dstPath, sizeof(dstPath), "%s/%016llX.tga", IconsDirectory,
             static_cast<unsigned long long>(titleId));

    FILE* dst = fopen(dstPath, "wb");
    if (!dst) {
        fclose(src);
        return;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(dst);
    fclose(src);
}

bool loadImage(uint64_t titleId, Renderer::ImageHandle& outHandle, bool checkRenderer = true)
{
    outHandle = Renderer::INVALID_IMAGE;

    if (checkRenderer && !Renderer::SupportsImages()) {
        return false;
    }

    // Try SD card cache first
    char sdPath[160];
    snprintf(sdPath, sizeof(sdPath), "%s/%016llX.tga", IconsDirectory,
             static_cast<unsigned long long>(titleId));
    if (loadImageFromFile(sdPath, outHandle)) {
        return true;
    }

    // Try title meta directory
    char metaPath[256];
    if (ACPGetTitleMetaDir(titleId, metaPath, sizeof(metaPath)) == ACP_RESULT_SUCCESS) {
        char iconPath[280];
        snprintf(iconPath, sizeof(iconPath), "%s/iconTex.tga", metaPath);
        if (loadImageFromFile(iconPath, outHandle)) {
            copyFileToSDCard(iconPath, titleId);
            return true;
        }
    }

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

    mkdir(ConfigDirectory, 0755);
    mkdir(IconsDirectory, 0755);

    sInitialized = true;
    return true;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    // Free all loaded images
    for (auto& pair : sRequests) {
        if (pair.second.handle) {
            freeImage(pair.second.handle);
            pair.second.handle = Renderer::INVALID_IMAGE;
        }
    }

    sRequests.clear();
    sLoadQueue.clear();
    sCacheOrder.clear();
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
        // Status is FAILED or NOT_REQUESTED - will re-queue below
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

    // Free all loaded images
    for (auto& pair : sRequests) {
        if (pair.second.handle) {
            freeImage(pair.second.handle);
        }
    }

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
        // Free the image memory
        if (it->second.handle) {
            freeImage(it->second.handle);
        }
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
        if (loadImage(titleId, handle, false)) {
            it->second.status = Status::READY;
            it->second.handle = handle;
            touchCache(titleId);
        } else {
            it->second.status = Status::FAILED;
            it->second.handle = Renderer::INVALID_IMAGE;
        }
    }
}

} // namespace ImageLoader
