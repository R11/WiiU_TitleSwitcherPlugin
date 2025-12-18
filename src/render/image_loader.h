/**
 * Asynchronous Image Loader
 *
 * Handles background loading of images with caching and priority queue.
 * Designed for loading game icons without blocking the UI.
 *
 * FEATURES:
 * ---------
 * 1. Async loading: Images load in background, UI remains responsive
 * 2. Priority queue: Currently selected title's icon loads first
 * 3. LRU cache: Recently viewed icons stay in memory
 * 4. Placeholder support: Show placeholder while loading
 *
 * USAGE:
 * ------
 *   // Initialize (call once at startup)
 *   ImageLoader::Init();
 *
 *   // Request an image (returns immediately)
 *   ImageLoader::Request(titleId, ImageLoader::Priority::HIGH);
 *
 *   // Check if ready and get handle
 *   if (ImageLoader::IsReady(titleId)) {
 *       Renderer::ImageHandle img = ImageLoader::Get(titleId);
 *       Renderer::DrawImage(x, y, img, 64, 64);
 *   } else {
 *       Renderer::DrawPlaceholder(x, y, 64, 64, 0x333333FF);
 *   }
 *
 *   // Update priority when selection changes
 *   ImageLoader::SetPriority(newTitleId, ImageLoader::Priority::HIGH);
 *   ImageLoader::SetPriority(oldTitleId, ImageLoader::Priority::NORMAL);
 *
 *   // Process loading queue (call each frame)
 *   ImageLoader::Update();
 *
 *   // Cleanup
 *   ImageLoader::Shutdown();
 *
 * CACHE BEHAVIOR:
 * ---------------
 * - Icons are cached in memory after loading
 * - LRU eviction when cache is full
 * - Cache size is configurable
 */

#pragma once

#include "renderer.h"
#include <cstdint>

namespace ImageLoader {

// =============================================================================
// Constants
// =============================================================================

// Maximum number of images to keep in cache
constexpr int DEFAULT_CACHE_SIZE = 32;

// Icon dimensions (Wii U standard)
constexpr int ICON_WIDTH = 128;
constexpr int ICON_HEIGHT = 128;

// =============================================================================
// Priority Levels
// =============================================================================

/**
 * Loading priority for image requests.
 */
enum class Priority {
    LOW,        // Background prefetch
    NORMAL,     // Standard loading
    HIGH        // Currently selected, load ASAP
};

// =============================================================================
// Status
// =============================================================================

/**
 * Status of an image request.
 */
enum class Status {
    NOT_REQUESTED,  // Never requested
    QUEUED,         // In loading queue
    LOADING,        // Currently being loaded
    READY,          // Loaded and available
    FAILED          // Load failed (file not found, etc.)
};

// =============================================================================
// Lifecycle
// =============================================================================

/**
 * Initialize the image loader.
 * Call once at startup.
 *
 * @param cacheSize Maximum number of images to cache (0 = default)
 * @return true if initialization succeeded
 */
bool Init(int cacheSize = DEFAULT_CACHE_SIZE);

/**
 * Shut down the image loader.
 * Releases all cached images and stops background loading.
 */
void Shutdown();

/**
 * Process the loading queue.
 * Call this once per frame to make progress on background loading.
 * Loads one image per call to avoid blocking.
 */
void Update();

// =============================================================================
// Image Requests
// =============================================================================

/**
 * Request an image for a title.
 * If already cached, does nothing. Otherwise adds to loading queue.
 *
 * @param titleId  The title ID to load icon for
 * @param priority Loading priority
 */
void Request(uint64_t titleId, Priority priority = Priority::NORMAL);

/**
 * Cancel a pending request.
 * If already loaded, the image remains in cache.
 *
 * @param titleId The title ID to cancel
 */
void Cancel(uint64_t titleId);

/**
 * Update the priority of a pending request.
 * Use this when the user's selection changes.
 *
 * @param titleId  The title ID to reprioritize
 * @param priority New priority level
 */
void SetPriority(uint64_t titleId, Priority priority);

// =============================================================================
// Image Access
// =============================================================================

/**
 * Get the status of an image request.
 *
 * @param titleId The title ID to check
 * @return Current status
 */
Status GetStatus(uint64_t titleId);

/**
 * Check if an image is ready to use.
 *
 * @param titleId The title ID to check
 * @return true if Get() will return a valid handle
 */
bool IsReady(uint64_t titleId);

/**
 * Get the image handle for a title.
 * Only valid if IsReady() returns true.
 *
 * @param titleId The title ID
 * @return Image handle, or INVALID_IMAGE if not ready
 */
Renderer::ImageHandle Get(uint64_t titleId);

/**
 * Get the last error message for debugging.
 *
 * @return Pointer to static error string (do not free)
 */
const char* GetLastError();

// =============================================================================
// Cache Management
// =============================================================================

/**
 * Clear the entire cache.
 * All loaded images are released.
 */
void ClearCache();

/**
 * Remove a specific image from cache.
 *
 * @param titleId The title ID to evict
 */
void Evict(uint64_t titleId);

/**
 * Get the number of images currently cached.
 *
 * @return Cache entry count
 */
int GetCacheCount();

/**
 * Get the maximum cache size.
 *
 * @return Maximum number of cached images
 */
int GetCacheCapacity();

// =============================================================================
// Prefetching
// =============================================================================

/**
 * Prefetch icons for nearby titles.
 * Call this with title IDs near the current selection to preload them.
 *
 * @param titleIds  Array of title IDs to prefetch
 * @param count     Number of IDs in array
 */
void Prefetch(const uint64_t* titleIds, int count);

} // namespace ImageLoader
