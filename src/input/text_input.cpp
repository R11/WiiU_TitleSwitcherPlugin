/**
 * Text Input Component Implementation
 *
 * See text_input.h for usage documentation.
 */

#include "text_input.h"
#include "buttons.h"
#include "../render/renderer.h"

#include <cstring>

namespace TextInput {

// =============================================================================
// Character Sets
// =============================================================================
// These define the valid characters for each library type.
// Characters are ordered for intuitive cycling (space first, then numbers,
// then letters).
// =============================================================================

namespace {

// Alphanumeric: A-Z, a-z, 0-9, space (A first so pressing Up shows A)
const char CHARSET_ALPHA_NUMERIC[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";

// Hexadecimal: 0-9, A-F
const char CHARSET_HEX[] = "0123456789ABCDEF";

// Numeric: 0-9
const char CHARSET_NUMERIC[] = "0123456789";

} // anonymous namespace

// =============================================================================
// Field Implementation
// =============================================================================

Field::Field()
    : mMaxLength(0)
    , mCursorPos(0)
    , mLibrary(Library::ALPHA_NUMERIC)
    , mInitialized(false)
    , mRepeatDelay(20)      // ~333ms at 60fps before repeat starts
    , mRepeatInterval(4)    // ~67ms between repeats (fast scroll)
    , mHoldFrames(0)
    , mLastHeld(0)
{
    memset(mChars, ' ', MAX_LENGTH);
    mChars[MAX_LENGTH] = '\0';
}

void Field::Init(int maxLength, Library library)
{
    // Clamp length to valid range
    if (maxLength < 1) maxLength = 1;
    if (maxLength > MAX_LENGTH) maxLength = MAX_LENGTH;

    mMaxLength = maxLength;
    mLibrary = library;
    mCursorPos = 0;
    mInitialized = true;

    // Fill with spaces
    memset(mChars, ' ', MAX_LENGTH);
    mChars[MAX_LENGTH] = '\0';
}

void Field::Clear()
{
    mCursorPos = 0;
    memset(mChars, ' ', MAX_LENGTH);
    mChars[MAX_LENGTH] = '\0';
}

void Field::SetValue(const char* value)
{
    if (!mInitialized || !value) return;

    // Clear first
    memset(mChars, ' ', mMaxLength);

    // Copy characters, validating each one
    const char* charSet = getCharSet();
    int len = strlen(value);
    if (len > mMaxLength) len = mMaxLength;

    for (int i = 0; i < len; i++) {
        char c = value[i];
        // Check if character is in our set
        if (strchr(charSet, c) != nullptr) {
            mChars[i] = c;
        } else if (mLibrary == Library::HEX && c >= 'a' && c <= 'f') {
            // Convert lowercase hex to uppercase
            mChars[i] = c - 'a' + 'A';
        }
        // Invalid characters become spaces (default)
    }
}

void Field::GetValue(char* outValue, int maxLen) const
{
    if (!outValue || maxLen <= 0) return;

    // Copy up to mMaxLength characters
    int copyLen = mMaxLength;
    if (copyLen >= maxLen) copyLen = maxLen - 1;

    strncpy(outValue, mChars, copyLen);
    outValue[copyLen] = '\0';

    // Trim trailing spaces
    int end = copyLen - 1;
    while (end >= 0 && outValue[end] == ' ') {
        outValue[end] = '\0';
        end--;
    }
}

int Field::GetLength() const
{
    // Find the last non-space character
    int len = 0;
    for (int i = 0; i < mMaxLength; i++) {
        if (mChars[i] != ' ') {
            len = i + 1;
        }
    }
    return len;
}

bool Field::IsEmpty() const
{
    for (int i = 0; i < mMaxLength; i++) {
        if (mChars[i] != ' ') {
            return false;
        }
    }
    return true;
}

void Field::Render(int col, int row) const
{
    if (!mInitialized) return;

    // Build the character display line
    // Show characters with spacing for readability
    char displayLine[MAX_LENGTH * 2 + 1];
    int pos = 0;

    for (int i = 0; i < mMaxLength; i++) {
        displayLine[pos++] = mChars[i];
        if (i < mMaxLength - 1) {
            displayLine[pos++] = ' ';  // Space between characters
        }
    }
    displayLine[pos] = '\0';

    // Build the cursor line
    char cursorLine[MAX_LENGTH * 2 + 1];
    memset(cursorLine, ' ', sizeof(cursorLine) - 1);
    cursorLine[mCursorPos * 2] = '^';  // Cursor under current position
    cursorLine[mMaxLength * 2 - 1] = '\0';

    // Render both lines
    Renderer::DrawText(col, row, displayLine);
    Renderer::DrawText(col, row + 1, cursorLine);
}

Result Field::HandleInput(uint32_t pressed, uint32_t held)
{
    if (!mInitialized) return Result::CANCELLED;

    // Check for confirm/cancel first
    if (Buttons::Actions::INPUT_CONFIRM.Pressed(pressed)) {
        return Result::CONFIRMED;
    }

    if (Buttons::Actions::INPUT_CANCEL.Pressed(pressed)) {
        return Result::CANCELLED;
    }

    // Track hold timing for repeat
    // We track Up/Down for character cycling repeat
    uint32_t repeatMask = Buttons::Actions::INPUT_CHAR_UP.input |
                          Buttons::Actions::INPUT_CHAR_DOWN.input;
    uint32_t relevantHeld = held & repeatMask;

    bool shouldRepeat = false;
    if (relevantHeld != 0) {
        if (relevantHeld == mLastHeld) {
            // Same button still held
            mHoldFrames++;
            if (mHoldFrames >= mRepeatDelay) {
                // In repeat mode - check interval
                int framesSinceDelay = mHoldFrames - mRepeatDelay;
                if (framesSinceDelay % mRepeatInterval == 0) {
                    shouldRepeat = true;
                }
            }
        } else {
            // Different button or just started holding
            mHoldFrames = 0;
        }
    } else {
        // No relevant button held
        mHoldFrames = 0;
    }
    mLastHeld = relevantHeld;

    // Character cycling (Up/Down) - triggered or repeat
    bool cycleUp = Buttons::Actions::INPUT_CHAR_UP.Pressed(pressed) ||
                   (shouldRepeat && Buttons::Actions::INPUT_CHAR_UP.Held(held));
    bool cycleDown = Buttons::Actions::INPUT_CHAR_DOWN.Pressed(pressed) ||
                     (shouldRepeat && Buttons::Actions::INPUT_CHAR_DOWN.Held(held));

    if (cycleUp) {
        cycleChar(1);  // Next character
    }
    if (cycleDown) {
        cycleChar(-1);  // Previous character
    }

    // Cursor movement - A/B or Left/Right
    bool moveRight = Buttons::Actions::INPUT_RIGHT.Pressed(pressed) ||
                     Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed);  // Right D-pad
    bool moveLeft = Buttons::Actions::INPUT_LEFT.Pressed(pressed) ||
                    Buttons::Actions::NAV_SKIP_UP.Pressed(pressed);     // Left D-pad

    if (moveRight) {
        if (mCursorPos < mMaxLength - 1) {
            mCursorPos++;
        }
    }
    if (moveLeft) {
        if (mCursorPos > 0) {
            mCursorPos--;
        }
    }

    // Delete character (X)
    if (Buttons::Actions::INPUT_DELETE.Pressed(pressed)) {
        // Shift characters left from cursor position
        for (int i = mCursorPos; i < mMaxLength - 1; i++) {
            mChars[i] = mChars[i + 1];
        }
        // Fill last position with space
        mChars[mMaxLength - 1] = ' ';
    }

    return Result::ACTIVE;
}

int Field::GetCursorPosition() const
{
    return mCursorPos;
}

void Field::SetCursorPosition(int pos)
{
    if (pos < 0) pos = 0;
    if (pos >= mMaxLength) pos = mMaxLength - 1;
    mCursorPos = pos;
}

const char* Field::getCharSet() const
{
    switch (mLibrary) {
        case Library::HEX:
            return CHARSET_HEX;
        case Library::NUMERIC:
            return CHARSET_NUMERIC;
        case Library::ALPHA_NUMERIC:
        default:
            return CHARSET_ALPHA_NUMERIC;
    }
}

int Field::getCharSetLength() const
{
    return strlen(getCharSet());
}

int Field::findCharIndex(char c) const
{
    const char* charSet = getCharSet();
    const char* pos = strchr(charSet, c);
    if (pos != nullptr) {
        return pos - charSet;
    }
    return -1;
}

void Field::cycleChar(int direction)
{
    const char* charSet = getCharSet();
    int setLen = getCharSetLength();

    // Find current character's index in the set
    int currentIdx = findCharIndex(mChars[mCursorPos]);

    if (currentIdx < 0) {
        // Current char not in set - start at beginning
        currentIdx = 0;
    } else {
        // Move in the specified direction with wrapping
        currentIdx += direction;

        // Wrap around at boundaries
        if (currentIdx < 0) {
            currentIdx = setLen - 1;  // Wrap to end
        }
        if (currentIdx >= setLen) {
            currentIdx = 0;  // Wrap to beginning
        }
    }

    // Set the new character
    mChars[mCursorPos] = charSet[currentIdx];
}

// =============================================================================
// Convenience Functions
// =============================================================================

const char* GetLibraryName(Library lib)
{
    switch (lib) {
        case Library::HEX:
            return "Hex";
        case Library::NUMERIC:
            return "Numeric";
        case Library::ALPHA_NUMERIC:
        default:
            return "Alphanumeric";
    }
}

} // namespace TextInput
