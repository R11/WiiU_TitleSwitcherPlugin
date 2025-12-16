/**
 * TextInput Stub for Desktop Preview
 *
 * Provides stub text input - enough to compile menu rendering code.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace TextInput {

constexpr int MAX_LENGTH = 32;

enum class Library {
    ALPHA_NUMERIC,
    HEX,
    NUMERIC
};

enum class Result {
    ACTIVE,
    CONFIRMED,
    CANCELLED
};

class Field {
public:
    Field() : mMaxLength(0), mCursorPos(0), mInitialized(false) {
        mChars[0] = '\0';
    }

    void Init(int maxLength, Library library = Library::ALPHA_NUMERIC) {
        (void)library;
        mMaxLength = maxLength;
        mCursorPos = 0;
        mInitialized = true;
        Clear();
    }

    void Clear() {
        for (int i = 0; i <= mMaxLength; i++) {
            mChars[i] = ' ';
        }
        mChars[mMaxLength] = '\0';
        mCursorPos = 0;
    }

    void SetValue(const char* value) {
        Clear();
        int len = strlen(value);
        if (len > mMaxLength) len = mMaxLength;
        for (int i = 0; i < len; i++) {
            mChars[i] = value[i];
        }
    }

    void GetValue(char* outValue, int maxLen) const {
        int len = GetLength();
        if (len >= maxLen) len = maxLen - 1;
        strncpy(outValue, mChars, len);
        outValue[len] = '\0';
    }

    int GetLength() const {
        int len = mMaxLength;
        while (len > 0 && mChars[len - 1] == ' ') len--;
        return len;
    }

    bool IsEmpty() const {
        return GetLength() == 0;
    }

    void Render(int col, int row) const {
        // Stub - would render the input field
        (void)col;
        (void)row;
    }

    Result HandleInput(uint32_t pressed, uint32_t held = 0) {
        (void)pressed;
        (void)held;
        return Result::ACTIVE;
    }

    int GetCursorPosition() const { return mCursorPos; }
    void SetCursorPosition(int pos) { mCursorPos = pos; }

private:
    char mChars[MAX_LENGTH + 1];
    int mMaxLength;
    int mCursorPos;
    bool mInitialized;
};

inline const char* GetLibraryName(Library lib) {
    switch (lib) {
        case Library::ALPHA_NUMERIC: return "Alphanumeric";
        case Library::HEX: return "Hex";
        case Library::NUMERIC: return "Numeric";
        default: return "Unknown";
    }
}

} // namespace TextInput
