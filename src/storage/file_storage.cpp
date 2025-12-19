// Low-level file I/O operations

#include "file_storage.h"

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace FileStorage {

bool ReadFile(const char* path, uint8_t** outData, size_t* outSize)
{
    if (!path || !outData || !outSize) {
        return false;
    }

    *outData = nullptr;
    *outSize = 0;

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(file);
        return false;
    }

    uint8_t* buffer = static_cast<uint8_t*>(malloc(fileSize));
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    fclose(file);

    if (bytesRead != static_cast<size_t>(fileSize)) {
        free(buffer);
        return false;
    }

    *outData = buffer;
    *outSize = bytesRead;
    return true;
}

bool WriteFile(const char* path, const uint8_t* data, size_t size)
{
    if (!path || !data || size == 0) {
        return false;
    }

    FILE* file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    size_t bytesWritten = fwrite(data, 1, size, file);
    fclose(file);

    return bytesWritten == size;
}

bool CopyFile(const char* srcPath, const char* dstPath)
{
    if (!srcPath || !dstPath) {
        return false;
    }

    FILE* src = fopen(srcPath, "rb");
    if (!src) {
        return false;
    }

    FILE* dst = fopen(dstPath, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    char buffer[4096];
    size_t bytesRead;
    bool success = true;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytesRead, dst) != bytesRead) {
            success = false;
            break;
        }
    }

    fclose(dst);
    fclose(src);
    return success;
}

bool Exists(const char* path)
{
    if (!path) {
        return false;
    }

    struct stat st;
    return stat(path, &st) == 0;
}

bool CreateDir(const char* path)
{
    if (!path) {
        return false;
    }

    return mkdir(path, 0755) == 0;
}

} // namespace FileStorage
