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

// libgd for image loading
#include <gd.h>

// Wii U SDK for title meta directory
#include <nn/acp/title.h>

// Notifications for diagnostic logging
#include <notifications/notifications.h>

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

    // Open file
    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 8) {
        fclose(file);
        return false;
    }

    // Read file into memory
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

    // Detect format and load with libgd
    gdImagePtr gdImg = nullptr;

    if (buffer[0] == 0x89 && buffer[1] == 'P' && buffer[2] == 'N' && buffer[3] == 'G') {
        // PNG
        gdImg = gdImageCreateFromPngPtr(fileSize, buffer);
    } else if (buffer[0] == 0xFF && buffer[1] == 0xD8) {
        // JPEG
        gdImg = gdImageCreateFromJpegPtr(fileSize, buffer);
    } else if (buffer[0] == 'B' && buffer[1] == 'M') {
        // BMP
        gdImg = gdImageCreateFromBmpPtr(fileSize, buffer);
    } else if (buffer[0] == 0x00) {
        // TGA (usually starts with 0x00)
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

/**
 * Load icon for a title using ACPGetTitleMetaDir.
 */
bool loadImage(uint64_t titleId, Renderer::ImageHandle& outHandle)
{
    outHandle = Renderer::INVALID_IMAGE;

    // Cannot load images if renderer doesn't support them
    if (!Renderer::SupportsImages()) {
        NotificationModule_AddInfoNotification("ImageLoader: SupportsImages=false");
        return false;
    }

    // Get meta directory for this title
    char metaPath[256];

    ACPResult result = ACPGetTitleMetaDir(titleId, metaPath, sizeof(metaPath));
    if (result != ACP_RESULT_SUCCESS) {
        char msg[64];
        snprintf(msg, sizeof(msg), "ImageLoader: ACPGetTitleMetaDir failed=%d", (int)result);
        NotificationModule_AddInfoNotification(msg);
        return false;
    }

    // Construct path to icon
    char iconPath[280];
    snprintf(iconPath, sizeof(iconPath), "%s/iconTex.tga", metaPath);

    // Load the image
    bool loaded = loadImageFromFile(iconPath, outHandle);
    if (!loaded) {
        char msg[96];
        snprintf(msg, sizeof(msg), "ImageLoader: loadImageFromFile failed: %.60s", iconPath);
        NotificationModule_AddInfoNotification(msg);
    }
    return loaded;
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

} // namespace ImageLoader
