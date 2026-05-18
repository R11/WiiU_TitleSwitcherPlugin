// Image storage with pluggable sources (memory cache, SD card, NAND)

#pragma once

#include "../render/renderer.h"
#include <cstdint>
#include <cstddef>

namespace ImageStore {

// Storage sources
enum class Source {
    MEMORY,   // In-memory LRU cache
    SD_CARD,  // SD card icon cache
    NAND      // Title meta directory (read-only)
};

// Initialize the image store
void Init(int memoryCacheSize = 50);

// Shutdown and free all cached images
void Shutdown();

// Configure sources
void SetSourceEnabled(Source src, bool enabled);
void SetWriteEnabled(Source src, bool enabled);
bool IsSourceEnabled(Source src);
bool IsWriteEnabled(Source src);

// Load image for a title (tries enabled sources in order)
// Returns true if image was loaded, false if not found
bool Load(uint64_t titleId, Renderer::ImageHandle& outHandle);

// Check if image is in memory cache
bool IsInMemoryCache(uint64_t titleId);

// Get image from memory cache only (no file I/O)
Renderer::ImageHandle GetFromMemoryCache(uint64_t titleId);

// Store image in memory cache
void StoreInMemoryCache(uint64_t titleId, Renderer::ImageHandle handle);

// Remove from memory cache
void RemoveFromMemoryCache(uint64_t titleId);

// Clear entire memory cache
void ClearMemoryCache();

// Get memory cache stats
int GetMemoryCacheCount();
int GetMemoryCacheCapacity();

// SD card icon paths
const char* GetIconsDirectory();
void GetIconPath(uint64_t titleId, char* outPath, size_t maxLen);

} // namespace ImageStore
