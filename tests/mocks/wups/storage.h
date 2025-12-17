/**
 * WUPS Storage API Mock for Unit Testing
 *
 * Provides in-memory storage that mimics the WUPS Storage API behavior
 * without requiring actual SD card access.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// WUPS Storage error codes (matching real values)
enum WUPSStorageError {
    WUPS_STORAGE_ERROR_SUCCESS = 0,
    WUPS_STORAGE_ERROR_NOT_FOUND = -1,
    WUPS_STORAGE_ERROR_INVALID_ARGS = -2,
    WUPS_STORAGE_ERROR_BUFFER_TOO_SMALL = -3
};

// Mock storage - in-memory key-value store for testing
namespace MockStorage {

inline std::map<std::string, int32_t> intStore;
inline std::map<std::string, std::vector<uint8_t>> binaryStore;

// Reset all stored data (call in test SetUp)
inline void Reset() {
    intStore.clear();
    binaryStore.clear();
}

} // namespace MockStorage

// WUPS Storage API mock implementations

inline WUPSStorageError WUPSStorageAPI_GetInt(void*, const char* key, int32_t* out) {
    if (!key || !out) return WUPS_STORAGE_ERROR_INVALID_ARGS;

    auto it = MockStorage::intStore.find(key);
    if (it == MockStorage::intStore.end()) {
        return WUPS_STORAGE_ERROR_NOT_FOUND;
    }
    *out = it->second;
    return WUPS_STORAGE_ERROR_SUCCESS;
}

inline WUPSStorageError WUPSStorageAPI_StoreInt(void*, const char* key, int32_t value) {
    if (!key) return WUPS_STORAGE_ERROR_INVALID_ARGS;

    MockStorage::intStore[key] = value;
    return WUPS_STORAGE_ERROR_SUCCESS;
}

inline WUPSStorageError WUPSStorageAPI_GetBinary(void*, const char* key, void* data,
                                                  uint32_t maxSize, uint32_t* outSize) {
    if (!key || !data) return WUPS_STORAGE_ERROR_INVALID_ARGS;

    auto it = MockStorage::binaryStore.find(key);
    if (it == MockStorage::binaryStore.end()) {
        return WUPS_STORAGE_ERROR_NOT_FOUND;
    }

    uint32_t copySize = std::min(maxSize, static_cast<uint32_t>(it->second.size()));
    std::memcpy(data, it->second.data(), copySize);
    if (outSize) *outSize = copySize;

    return WUPS_STORAGE_ERROR_SUCCESS;
}

inline WUPSStorageError WUPSStorageAPI_StoreBinary(void*, const char* key,
                                                    const void* data, uint32_t size) {
    if (!key || !data) return WUPS_STORAGE_ERROR_INVALID_ARGS;

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    MockStorage::binaryStore[key] = std::vector<uint8_t>(bytes, bytes + size);
    return WUPS_STORAGE_ERROR_SUCCESS;
}

inline void WUPSStorageAPI_SaveStorage(bool) {
    // No-op for tests - data is already in memory
}
