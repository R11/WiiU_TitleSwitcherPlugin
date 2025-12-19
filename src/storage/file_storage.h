// Low-level file I/O operations for SD card access

#pragma once

#include <cstdint>
#include <cstddef>

namespace FileStorage {

// Read entire file into buffer. Caller must free() the returned data.
bool ReadFile(const char* path, uint8_t** outData, size_t* outSize);

// Write buffer to file
bool WriteFile(const char* path, const uint8_t* data, size_t size);

// Copy file from srcPath to dstPath
bool CopyFile(const char* srcPath, const char* dstPath);

// Check if file exists
bool Exists(const char* path);

// Create directory (single level, not recursive)
bool CreateDir(const char* path);

} // namespace FileStorage
