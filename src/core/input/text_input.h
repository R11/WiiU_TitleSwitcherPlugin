/**
 * Text Input Component
 *
 * A reusable text input field for entering strings character-by-character.
 * Designed for use without a keyboard - users cycle through characters
 * using Up/Down and move between positions with A/B.
 *
 * APPEARANCE:
 * -----------
 * The input displays all character slots with a cursor underneath:
 *
 *   H E L L O
 *       ^
 *
 * Empty slots show as spaces. The cursor (^) indicates the current position.
 *
 * CONTROLS:
 * ---------
 * - Up/Down: Cycle through valid characters at cursor position
 * - A: Move cursor right
 * - B: Move cursor left
 * - X: Delete character at cursor (shift remaining left)
 * - Start (+): Confirm input
 * - Select (-): Cancel input
 *
 * CHARACTER LIBRARIES:
 * --------------------
 * Different input types can use different character sets:
 *
 * - ALPHA_NUMERIC: a-z, A-Z, 0-9, space (for names, text)
 * - HEX: 0-9, A-F (for color values)
 * - NUMERIC: 0-9 (for numbers only)
 *
 * USAGE:
 * ------
 *   TextInput input;
 *   input.Init(6, TextInput::Library::HEX);  // 6-char hex input
 *   input.SetValue("FF00FF");                 // Optional initial value
 *
 *   // In your render/input loop:
 *   input.Render(col, row);                   // Draw the input
 *   TextInput::Result result = input.HandleInput(pressed);
 *
 *   if (result == TextInput::Result::CONFIRMED) {
 *       char value[32];
 *       input.GetValue(value, sizeof(value));
 *       // Use the value...
 *   }
 */

#pragma once

#include <cstdint>

namespace TextInput {

// =============================================================================
// Constants
// =============================================================================

// Maximum input length supported
constexpr int MAX_LENGTH = 32;

// =============================================================================
// Character Libraries
// =============================================================================

/**
 * Defines which characters are valid for an input field.
 */
enum class Library {
    // Letters (upper and lower), numbers, space
    // Order: space, 0-9, A-Z, a-z
    ALPHA_NUMERIC,

    // Hexadecimal digits only
    // Order: 0-9, A-F
    HEX,

    // Numbers only
    // Order: 0-9
    NUMERIC
};

// =============================================================================
// Input Result
// =============================================================================

/**
 * Result of handling input for the text field.
 */
enum class Result {
    // Input is still active, continue processing
    ACTIVE,

    // User confirmed the input (Start pressed)
    CONFIRMED,

    // User cancelled the input (Select pressed)
    CANCELLED
};

// =============================================================================
// TextInputField Class
// =============================================================================

/**
 * A single text input field instance.
 */
class Field {
public:
    /**
     * Default constructor - creates an uninitialized field.
     * Call Init() before using.
     */
    Field();

    /**
     * Initialize the input field.
     *
     * @param maxLength Maximum number of characters (1 to MAX_LENGTH)
     * @param library   Which character set to use
     */
    void Init(int maxLength, Library library = Library::ALPHA_NUMERIC);

    /**
     * Reset the field to empty state.
     * Keeps the same maxLength and library settings.
     */
    void Clear();

    /**
     * Set the current value.
     *
     * @param value String to set (will be truncated if too long)
     */
    void SetValue(const char* value);

    /**
     * Get the current value.
     *
     * @param outValue Buffer to receive the value
     * @param maxLen   Size of the buffer
     */
    void GetValue(char* outValue, int maxLen) const;

    /**
     * Get the current value length (excluding trailing spaces).
     *
     * @return Number of non-space characters from start
     */
    int GetLength() const;

    /**
     * Check if the field is empty (all spaces).
     *
     * @return true if no characters have been entered
     */
    bool IsEmpty() const;

    /**
     * Render the input field at the specified position.
     *
     * @param col Starting column
     * @param row Row to render on
     *
     * Renders two lines:
     * - Line 1: The characters
     * - Line 2: The cursor (^) under current position
     */
    void Render(int col, int row) const;

    /**
     * Handle input for this field.
     *
     * @param pressed Buttons pressed this frame (from VPADStatus.trigger)
     * @param held    Buttons held this frame (from VPADStatus.hold) for repeat
     * @return Result indicating if input should continue
     */
    Result HandleInput(uint32_t pressed, uint32_t held = 0);

    /**
     * Get the current cursor position.
     *
     * @return Cursor index (0 to maxLength-1)
     */
    int GetCursorPosition() const;

    /**
     * Set the cursor position.
     *
     * @param pos New position (clamped to valid range)
     */
    void SetCursorPosition(int pos);

private:
    // Current characters (space = empty)
    char mChars[MAX_LENGTH + 1];

    // Maximum length for this field
    int mMaxLength;

    // Current cursor position
    int mCursorPos;

    // Character library in use
    Library mLibrary;

    // Is the field initialized?
    bool mInitialized;

    // Repeat timing for held buttons
    int mRepeatDelay;      // Frames until repeat starts
    int mRepeatInterval;   // Frames between repeats
    int mHoldFrames;       // How long current button has been held
    uint32_t mLastHeld;    // Last held button state

    // Helper: Get the character set for current library
    const char* getCharSet() const;

    // Helper: Get the length of the character set
    int getCharSetLength() const;

    // Helper: Find index of character in set (-1 if not found)
    int findCharIndex(char c) const;

    // Helper: Cycle character at cursor position
    void cycleChar(int direction);
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * Get the display name for a library.
 *
 * @param lib The library
 * @return Human-readable name
 */
const char* GetLibraryName(Library lib);

} // namespace TextInput
