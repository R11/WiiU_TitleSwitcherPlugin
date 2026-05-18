/**
 * Text Input Component Implementation
 */

#include "text_input.h"
#include "buttons.h"
#include "../render/renderer.h"

#include <cstring>

namespace TextInput {

namespace {

const char CHARSET_ALPHA_NUMERIC[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
const char CHARSET_HEX[] = "0123456789ABCDEF";
const char CHARSET_NUMERIC[] = "0123456789";

}

Field::Field()
    : mMaxLength(0)
    , mCursorPos(0)
    , mLibrary(Library::ALPHA_NUMERIC)
    , mInitialized(false)
    , mRepeatDelay(20)
    , mRepeatInterval(4)
    , mHoldFrames(0)
    , mLastHeld(0)
{
    memset(mChars, ' ', MAX_LENGTH);
    mChars[MAX_LENGTH] = '\0';
}

void Field::Init(int maxLength, Library library)
{
    if (maxLength < 1) maxLength = 1;
    if (maxLength > MAX_LENGTH) maxLength = MAX_LENGTH;

    mMaxLength = maxLength;
    mLibrary = library;
    mCursorPos = 0;
    mInitialized = true;

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

    memset(mChars, ' ', mMaxLength);

    const char* charSet = getCharSet();
    int valueLength = strlen(value);
    if (valueLength > mMaxLength) valueLength = mMaxLength;

    for (int charIndex = 0; charIndex < valueLength; charIndex++) {
        char character = value[charIndex];
        if (strchr(charSet, character) != nullptr) {
            mChars[charIndex] = character;
        } else if (mLibrary == Library::HEX && character >= 'a' && character <= 'f') {
            mChars[charIndex] = character - 'a' + 'A';
        }
    }
}

void Field::GetValue(char* outputValue, int maxLength) const
{
    if (!outputValue || maxLength <= 0) return;

    int copyLength = mMaxLength;
    if (copyLength >= maxLength) copyLength = maxLength - 1;

    strncpy(outputValue, mChars, copyLength);
    outputValue[copyLength] = '\0';

    int endIndex = copyLength - 1;
    while (endIndex >= 0 && outputValue[endIndex] == ' ') {
        outputValue[endIndex] = '\0';
        endIndex--;
    }
}

int Field::GetLength() const
{
    int length = 0;
    for (int charIndex = 0; charIndex < mMaxLength; charIndex++) {
        if (mChars[charIndex] != ' ') {
            length = charIndex + 1;
        }
    }
    return length;
}

bool Field::IsEmpty() const
{
    for (int charIndex = 0; charIndex < mMaxLength; charIndex++) {
        if (mChars[charIndex] != ' ') {
            return false;
        }
    }
    return true;
}

void Field::Render(int column, int row) const
{
    if (!mInitialized) return;

    char displayLine[MAX_LENGTH * 2 + 1];
    int displayPosition = 0;

    for (int charIndex = 0; charIndex < mMaxLength; charIndex++) {
        displayLine[displayPosition++] = mChars[charIndex];
        if (charIndex < mMaxLength - 1) {
            displayLine[displayPosition++] = ' ';
        }
    }
    displayLine[displayPosition] = '\0';

    char cursorLine[MAX_LENGTH * 2 + 1];
    memset(cursorLine, ' ', sizeof(cursorLine) - 1);
    cursorLine[mCursorPos * 2] = '^';
    cursorLine[mMaxLength * 2 - 1] = '\0';

    Renderer::DrawText(column, row, displayLine);
    Renderer::DrawText(column, row + 1, cursorLine);
}

Result Field::HandleInput(uint32_t pressed, uint32_t held)
{
    if (!mInitialized) return Result::CANCELLED;

    if (Buttons::Actions::INPUT_CONFIRM.Pressed(pressed)) {
        return Result::CONFIRMED;
    }

    if (Buttons::Actions::INPUT_CANCEL.Pressed(pressed)) {
        return Result::CANCELLED;
    }

    uint32_t repeatMask = Buttons::Actions::INPUT_CHAR_UP.input |
                          Buttons::Actions::INPUT_CHAR_DOWN.input;
    uint32_t relevantHeldButtons = held & repeatMask;

    bool shouldRepeat = false;
    if (relevantHeldButtons != 0) {
        if (relevantHeldButtons == mLastHeld) {
            mHoldFrames++;
            if (mHoldFrames >= mRepeatDelay) {
                int framesSinceRepeatStart = mHoldFrames - mRepeatDelay;
                if (framesSinceRepeatStart % mRepeatInterval == 0) {
                    shouldRepeat = true;
                }
            }
        } else {
            mHoldFrames = 0;
        }
    } else {
        mHoldFrames = 0;
    }
    mLastHeld = relevantHeldButtons;

    bool cycleUp = Buttons::Actions::INPUT_CHAR_UP.Pressed(pressed) ||
                   (shouldRepeat && Buttons::Actions::INPUT_CHAR_UP.Held(held));
    bool cycleDown = Buttons::Actions::INPUT_CHAR_DOWN.Pressed(pressed) ||
                     (shouldRepeat && Buttons::Actions::INPUT_CHAR_DOWN.Held(held));

    if (cycleUp) {
        cycleChar(1);
    }
    if (cycleDown) {
        cycleChar(-1);
    }

    bool moveRight = Buttons::Actions::INPUT_RIGHT.Pressed(pressed) ||
                     Buttons::Actions::NAV_SKIP_DOWN.Pressed(pressed);
    bool moveLeft = Buttons::Actions::INPUT_LEFT.Pressed(pressed) ||
                    Buttons::Actions::NAV_SKIP_UP.Pressed(pressed);

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

    if (Buttons::Actions::INPUT_DELETE.Pressed(pressed)) {
        for (int shiftIndex = mCursorPos; shiftIndex < mMaxLength - 1; shiftIndex++) {
            mChars[shiftIndex] = mChars[shiftIndex + 1];
        }
        mChars[mMaxLength - 1] = ' ';
    }

    return Result::ACTIVE;
}

int Field::GetCursorPosition() const
{
    return mCursorPos;
}

void Field::SetCursorPosition(int position)
{
    if (position < 0) position = 0;
    if (position >= mMaxLength) position = mMaxLength - 1;
    mCursorPos = position;
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

int Field::findCharIndex(char character) const
{
    const char* charSet = getCharSet();
    const char* foundPosition = strchr(charSet, character);
    if (foundPosition != nullptr) {
        return foundPosition - charSet;
    }
    return -1;
}

void Field::cycleChar(int direction)
{
    const char* charSet = getCharSet();
    int setLength = getCharSetLength();

    int currentIndex = findCharIndex(mChars[mCursorPos]);

    if (currentIndex < 0) {
        currentIndex = 0;
    } else {
        currentIndex += direction;

        if (currentIndex < 0) {
            currentIndex = setLength - 1;
        }
        if (currentIndex >= setLength) {
            currentIndex = 0;
        }
    }

    mChars[mCursorPos] = charSet[currentIndex];
}

const char* GetLibraryName(Library library)
{
    switch (library) {
        case Library::HEX:
            return "Hex";
        case Library::NUMERIC:
            return "Numeric";
        case Library::ALPHA_NUMERIC:
        default:
            return "Alphanumeric";
    }
}

}
