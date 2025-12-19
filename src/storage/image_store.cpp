// Image storage implementation

#include "image_store.h"
#include "file_storage.h"

#include <map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gd.h>
#include <nn/acp/title.h>

namespace ImageStore {

namespace {

// SD card paths
constexpr const char* CONFIG_DIR = "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher";
constexpr const char* ICONS_DIR = "sd:/wiiu/environments/aroma/plugins/config/TitleSwitcher/icons";

// Source configuration
bool sMemoryEnabled = true;
bool sSDCardEnabled = true;
bool sNANDEnabled = true;
bool sSDCardWriteEnabled = false;  // Disabled by default until we debug the issue

// Memory cache
struct CacheEntry {
    Renderer::ImageHandle handle;
};
std::map<uint64_t, CacheEntry> sMemoryCache;
std::vector<uint64_t> sLRUOrder;  // Most recently used at end
int sCacheCapacity = 50;
bool sInitialized = false;

void touchLRU(uint64_t titleId)
{
    auto it = std::find(sLRUOrder.begin(), sLRUOrder.end(), titleId);
    if (it != sLRUOrder.end()) {
        sLRUOrder.erase(it);
    }
    sLRUOrder.push_back(titleId);
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
    while (static_cast<int>(sLRUOrder.size()) > sCacheCapacity && !sLRUOrder.empty()) {
        uint64_t oldest = sLRUOrder.front();
        sLRUOrder.erase(sLRUOrder.begin());

        auto it = sMemoryCache.find(oldest);
        if (it != sMemoryCache.end()) {
            freeImage(it->second.handle);
            sMemoryCache.erase(it);
        }
    }
}

// Parse image data into RGBA pixels
Renderer::ImageHandle parseImage(const uint8_t* data, size_t size)
{
    if (!data || size < 8) {
        return Renderer::INVALID_IMAGE;
    }

    // Detect format and load with libgd
    gdImagePtr gdImg = nullptr;
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        gdImg = gdImageCreateFromPngPtr(static_cast<int>(size), const_cast<uint8_t*>(data));
    } else if (data[0] == 0xFF && data[1] == 0xD8) {
        gdImg = gdImageCreateFromJpegPtr(static_cast<int>(size), const_cast<uint8_t*>(data));
    } else if (data[0] == 'B' && data[1] == 'M') {
        gdImg = gdImageCreateFromBmpPtr(static_cast<int>(size), const_cast<uint8_t*>(data));
    } else {
        gdImg = gdImageCreateFromTgaPtr(static_cast<int>(size), const_cast<uint8_t*>(data));
    }

    if (!gdImg) {
        return Renderer::INVALID_IMAGE;
    }

    int width = gdImageSX(gdImg);
    int height = gdImageSY(gdImg);

    uint32_t* pixels = static_cast<uint32_t*>(malloc(width * height * sizeof(uint32_t)));
    if (!pixels) {
        gdImageDestroy(gdImg);
        return Renderer::INVALID_IMAGE;
    }

    // Convert to RGBA
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel = gdImageGetPixel(gdImg, x, y);
            uint8_t r = gdImageRed(gdImg, pixel);
            uint8_t g = gdImageGreen(gdImg, pixel);
            uint8_t b = gdImageBlue(gdImg, pixel);
            uint8_t a = 255 - (gdImageAlpha(gdImg, pixel) * 2);
            pixels[y * width + x] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }

    gdImageDestroy(gdImg);

    Renderer::ImageData* imageData = new Renderer::ImageData();
    imageData->pixels = pixels;
    imageData->width = width;
    imageData->height = height;

    return imageData;
}

// Try loading from SD card
bool loadFromSDCard(uint64_t titleId, Renderer::ImageHandle& outHandle)
{
    char path[160];
    GetIconPath(titleId, path, sizeof(path));

    uint8_t* data = nullptr;
    size_t size = 0;
    if (!FileStorage::ReadFile(path, &data, &size)) {
        return false;
    }

    outHandle = parseImage(data, size);
    free(data);

    return outHandle != Renderer::INVALID_IMAGE;
}

// Try loading from NAND (title meta directory)
bool loadFromNAND(uint64_t titleId, Renderer::ImageHandle& outHandle, char* outNANDPath, size_t maxPathLen)
{
    char metaPath[256];
    if (ACPGetTitleMetaDir(titleId, metaPath, sizeof(metaPath)) != ACP_RESULT_SUCCESS) {
        return false;
    }

    char iconPath[280];
    snprintf(iconPath, sizeof(iconPath), "%s/iconTex.tga", metaPath);

    uint8_t* data = nullptr;
    size_t size = 0;
    if (!FileStorage::ReadFile(iconPath, &data, &size)) {
        return false;
    }

    outHandle = parseImage(data, size);
    free(data);

    if (outHandle != Renderer::INVALID_IMAGE && outNANDPath) {
        strncpy(outNANDPath, iconPath, maxPathLen - 1);
        outNANDPath[maxPathLen - 1] = '\0';
    }

    return outHandle != Renderer::INVALID_IMAGE;
}

} // anonymous namespace

void Init(int memoryCacheSize)
{
    if (sInitialized) {
        return;
    }

    sCacheCapacity = memoryCacheSize > 0 ? memoryCacheSize : 50;
    sMemoryCache.clear();
    sLRUOrder.clear();
    sInitialized = true;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    for (auto& pair : sMemoryCache) {
        freeImage(pair.second.handle);
    }
    sMemoryCache.clear();
    sLRUOrder.clear();
    sInitialized = false;
}

void SetSourceEnabled(Source src, bool enabled)
{
    switch (src) {
        case Source::MEMORY:   sMemoryEnabled = enabled; break;
        case Source::SD_CARD:  sSDCardEnabled = enabled; break;
        case Source::NAND:     sNANDEnabled = enabled; break;
    }
}

void SetWriteEnabled(Source src, bool enabled)
{
    if (src == Source::SD_CARD) {
        sSDCardWriteEnabled = enabled;
    }
}

bool IsSourceEnabled(Source src)
{
    switch (src) {
        case Source::MEMORY:   return sMemoryEnabled;
        case Source::SD_CARD:  return sSDCardEnabled;
        case Source::NAND:     return sNANDEnabled;
        default:               return false;
    }
}

bool IsWriteEnabled(Source src)
{
    if (src == Source::SD_CARD) {
        return sSDCardWriteEnabled;
    }
    return false;
}

bool Load(uint64_t titleId, Renderer::ImageHandle& outHandle)
{
    outHandle = Renderer::INVALID_IMAGE;

    if (!sInitialized) {
        return false;
    }

    // 1. Check memory cache
    if (sMemoryEnabled) {
        auto it = sMemoryCache.find(titleId);
        if (it != sMemoryCache.end()) {
            touchLRU(titleId);
            outHandle = it->second.handle;
            return true;
        }
    }

    // 2. Check SD card
    if (sSDCardEnabled) {
        if (loadFromSDCard(titleId, outHandle)) {
            if (sMemoryEnabled) {
                StoreInMemoryCache(titleId, outHandle);
            }
            return true;
        }
    }

    // 3. Check NAND
    if (sNANDEnabled) {
        char nandPath[280] = {0};
        if (loadFromNAND(titleId, outHandle, nandPath, sizeof(nandPath))) {
            if (sMemoryEnabled) {
                StoreInMemoryCache(titleId, outHandle);
            }
            // Copy to SD card for future loads
            if (sSDCardWriteEnabled && nandPath[0] != '\0') {
                char sdPath[160];
                GetIconPath(titleId, sdPath, sizeof(sdPath));
                FileStorage::CopyFile(nandPath, sdPath);
            }
            return true;
        }
    }

    return false;
}

bool IsInMemoryCache(uint64_t titleId)
{
    return sMemoryCache.find(titleId) != sMemoryCache.end();
}

Renderer::ImageHandle GetFromMemoryCache(uint64_t titleId)
{
    auto it = sMemoryCache.find(titleId);
    if (it != sMemoryCache.end()) {
        touchLRU(titleId);
        return it->second.handle;
    }
    return Renderer::INVALID_IMAGE;
}

void StoreInMemoryCache(uint64_t titleId, Renderer::ImageHandle handle)
{
    if (!sInitialized || !handle) {
        return;
    }

    auto it = sMemoryCache.find(titleId);
    if (it != sMemoryCache.end()) {
        touchLRU(titleId);
        return;
    }

    CacheEntry entry;
    entry.handle = handle;
    sMemoryCache[titleId] = entry;
    touchLRU(titleId);
    evictIfNeeded();
}

void RemoveFromMemoryCache(uint64_t titleId)
{
    auto it = sMemoryCache.find(titleId);
    if (it != sMemoryCache.end()) {
        freeImage(it->second.handle);
        sMemoryCache.erase(it);
    }

    auto lruIt = std::find(sLRUOrder.begin(), sLRUOrder.end(), titleId);
    if (lruIt != sLRUOrder.end()) {
        sLRUOrder.erase(lruIt);
    }
}

void ClearMemoryCache()
{
    for (auto& pair : sMemoryCache) {
        freeImage(pair.second.handle);
    }
    sMemoryCache.clear();
    sLRUOrder.clear();
}

int GetMemoryCacheCount()
{
    return static_cast<int>(sMemoryCache.size());
}

int GetMemoryCacheCapacity()
{
    return sCacheCapacity;
}

const char* GetIconsDirectory()
{
    return ICONS_DIR;
}

void GetIconPath(uint64_t titleId, char* outPath, size_t maxLen)
{
    snprintf(outPath, maxLen, "%s/%016llX.tga", ICONS_DIR,
             static_cast<unsigned long long>(titleId));
}

} // namespace ImageStore
