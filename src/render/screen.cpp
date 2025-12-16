/**
 * Screen Rendering Implementation
 *
 * See screen.h for usage documentation.
 *
 * ATTRIBUTION:
 * ------------
 * The initialization pattern used here (DC register save/restore,
 * MEMAllocFromMappedMemoryForGX2Ex, double clear+flip sequence) is derived from:
 *   - WiiUPluginLoaderBackend by Maschell (GPLv3)
 *   - https://github.com/wiiu-env/WiiUPluginLoaderBackend
 */

#include "screen.h"
#include "../utils/dc.h"

// Wii U SDK headers
#include <coreinit/screen.h>      // OSScreen* functions
#include <coreinit/cache.h>       // DCFlushRange
#include <coreinit/memory.h>      // Memory functions
#include <coreinit/systeminfo.h>  // OSEnableHomeButtonMenu
#include <gx2/event.h>            // GX2WaitForVsync
#include <memory/mappedmemory.h>  // MEMAllocFromMappedMemoryForGX2Ex

// Standard library
#include <cstdio>                 // vsnprintf
#include <cstdarg>                // va_list

namespace Screen {

// =============================================================================
// Internal State
// =============================================================================
// These variables track the screen system's state and resources.
// They are internal to this module and not exposed in the header.
// =============================================================================

namespace {

// Is the screen system initialized?
bool sInitialized = false;

// Was HOME button menu enabled before we took over?
// We need to restore this state when shutting down.
bool sHomeButtonWasEnabled = false;

// Saved display controller registers
// These are captured during Init() and restored during Shutdown()
// to return the display to its pre-takeover state.
DCRegisters sSavedDCRegs;

// Framebuffer pointers
// These are allocated using MEMAllocFromMappedMemoryForGX2Ex which
// provides memory that's accessible by both CPU and GPU.
void* sBufferTV  = nullptr;
void* sBufferDRC = nullptr;

// Framebuffer sizes (in bytes)
// The actual size depends on screen resolution and color format.
// OSScreenGetBufferSizeEx() returns the required size.
uint32_t sBufferSizeTV  = 0;
uint32_t sBufferSizeDRC = 0;

} // anonymous namespace

// =============================================================================
// Implementation
// =============================================================================

bool IsInitialized()
{
    return sInitialized;
}

bool Init()
{
    // Prevent double initialization
    if (sInitialized) {
        return true;
    }

    // -------------------------------------------------------------------------
    // Step 1: Save HOME button state
    // -------------------------------------------------------------------------
    // We disable the HOME button menu while our menu is open to prevent it
    // from interfering with our rendering. We save the original state so
    // we can restore it later.
    sHomeButtonWasEnabled = OSIsHomeButtonMenuEnabled();

    // -------------------------------------------------------------------------
    // Step 2: Save Display Controller registers
    // -------------------------------------------------------------------------
    // The DC registers control how framebuffer data is displayed. The game
    // has configured these for its rendering. We save them now so we can
    // restore them when we're done, ensuring the game's display returns
    // to exactly how it was.
    //
    // If we skip this step, the game's graphics will be corrupted when
    // our menu closes (wrong resolution, colors, or completely garbled).
    DCSaveRegisters(&sSavedDCRegs);

    // -------------------------------------------------------------------------
    // Step 3: Initialize OSScreen
    // -------------------------------------------------------------------------
    // OSScreen is a simple framebuffer-based rendering system provided by
    // the Wii U OS. It's designed for debug output but works well for
    // simple menu systems. It provides a character-based API for drawing
    // text, which is perfect for our needs.
    //
    // Reference: https://wut.devkitpro.org/group__coreinit__screen.html
    OSScreenInit();

    // -------------------------------------------------------------------------
    // Step 4: Get required buffer sizes
    // -------------------------------------------------------------------------
    // Each screen needs a framebuffer large enough to hold the entire screen
    // at the current resolution and color depth. OSScreenGetBufferSizeEx()
    // tells us exactly how much memory is needed.
    sBufferSizeTV  = OSScreenGetBufferSizeEx(SCREEN_TV);
    sBufferSizeDRC = OSScreenGetBufferSizeEx(SCREEN_DRC);

    // -------------------------------------------------------------------------
    // Step 5: Allocate framebuffers using GX2-compatible memory
    // -------------------------------------------------------------------------
    // CRITICAL: We MUST use MEMAllocFromMappedMemoryForGX2Ex() instead of
    // regular malloc(). Regular memory is not mapped into the GPU's address
    // space, which causes graphical corruption (garbled pixels, wrong colors).
    //
    // The 0x100 parameter is the alignment requirement - memory must be
    // aligned to 256 bytes for the GPU to access it correctly.
    //
    // Reference: https://github.com/wiiu-env/libmappedmemory
    sBufferTV  = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeTV, 0x100);
    sBufferDRC = MEMAllocFromMappedMemoryForGX2Ex(sBufferSizeDRC, 0x100);

    // Check if allocation succeeded
    if (!sBufferTV || !sBufferDRC) {
        // Allocation failed - clean up partial allocations
        if (sBufferTV) {
            MEMFreeToMappedMemory(sBufferTV);
            sBufferTV = nullptr;
        }
        if (sBufferDRC) {
            MEMFreeToMappedMemory(sBufferDRC);
            sBufferDRC = nullptr;
        }

        // Restore DC registers since we won't be using the screen
        DCRestoreRegisters(&sSavedDCRegs);

        return false;
    }

    // -------------------------------------------------------------------------
    // Step 6: Set up screen buffers
    // -------------------------------------------------------------------------
    // Tell OSScreen where our framebuffers are located.
    OSScreenSetBufferEx(SCREEN_TV, sBufferTV);
    OSScreenSetBufferEx(SCREEN_DRC, sBufferDRC);

    // -------------------------------------------------------------------------
    // Step 7: Initialize both front and back buffers
    // -------------------------------------------------------------------------
    // OSScreen uses double-buffering: one buffer is displayed while we draw
    // to the other. We need to clear BOTH buffers to prevent garbage from
    // appearing on the first frame.
    //
    // The pattern is: clear -> flush -> flip (x2)
    // This ensures both the front and back buffer are cleared.
    //
    // Clear to black (0x00000000) for the initial state.
    for (int i = 0; i < 2; i++) {
        OSScreenClearBufferEx(SCREEN_TV, 0);
        OSScreenClearBufferEx(SCREEN_DRC, 0);
        DCFlushRange(sBufferTV, sBufferSizeTV);
        DCFlushRange(sBufferDRC, sBufferSizeDRC);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    // -------------------------------------------------------------------------
    // Step 8: Enable screens
    // -------------------------------------------------------------------------
    // This activates the screen output. Without this call, nothing would
    // be displayed even though we're writing to the framebuffers.
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    // -------------------------------------------------------------------------
    // Step 9: Disable HOME button menu
    // -------------------------------------------------------------------------
    // While our menu is open, we don't want the HOME button to bring up
    // the system menu. This could interfere with our rendering and confuse
    // users since our menu is already visible.
    OSEnableHomeButtonMenu(false);

    sInitialized = true;
    return true;
}

void Shutdown()
{
    if (!sInitialized) {
        return;
    }

    // -------------------------------------------------------------------------
    // Step 1: Restore HOME button menu state
    // -------------------------------------------------------------------------
    OSEnableHomeButtonMenu(sHomeButtonWasEnabled);

    // -------------------------------------------------------------------------
    // Step 2: Restore Display Controller registers
    // -------------------------------------------------------------------------
    // This is the critical step that ensures the game's graphics return
    // to their original state. Without this, the game would have corrupted
    // display settings.
    DCRestoreRegisters(&sSavedDCRegs);

    // -------------------------------------------------------------------------
    // Step 3: Free framebuffers
    // -------------------------------------------------------------------------
    // Return the memory to the system. We use MEMFreeToMappedMemory()
    // because we allocated with MEMAllocFromMappedMemoryForGX2Ex().
    if (sBufferTV) {
        MEMFreeToMappedMemory(sBufferTV);
        sBufferTV = nullptr;
    }
    if (sBufferDRC) {
        MEMFreeToMappedMemory(sBufferDRC);
        sBufferDRC = nullptr;
    }

    sBufferSizeTV = 0;
    sBufferSizeDRC = 0;
    sInitialized = false;
}

void BeginFrame(uint32_t bgColor)
{
    if (!sInitialized) {
        return;
    }

    // -------------------------------------------------------------------------
    // Wait for vertical sync
    // -------------------------------------------------------------------------
    // This synchronizes our rendering with the display's refresh rate,
    // preventing screen tearing (where the top and bottom of the screen
    // show different frames).
    //
    // It also naturally limits our frame rate to 60fps, which is fine for
    // a menu interface and saves power.
    //
    // Reference: https://wut.devkitpro.org/group__gx2__event.html
    GX2WaitForVsync();

    // -------------------------------------------------------------------------
    // Clear both screen buffers
    // -------------------------------------------------------------------------
    // Fill the entire screen with the background color.
    // The color format is RGBA (0xRRGGBBAA).
    OSScreenClearBufferEx(SCREEN_TV, bgColor);
    OSScreenClearBufferEx(SCREEN_DRC, bgColor);
}

void EndFrame()
{
    if (!sInitialized) {
        return;
    }

    // -------------------------------------------------------------------------
    // Flush CPU cache
    // -------------------------------------------------------------------------
    // The CPU and GPU don't share a coherent cache, so after writing to
    // the framebuffer from CPU code, we must explicitly flush the cache
    // to ensure the GPU sees our changes.
    //
    // Without this, you might see stale data, flickering, or corruption.
    //
    // Reference: https://wut.devkitpro.org/group__coreinit__cache.html
    DCFlushRange(sBufferTV, sBufferSizeTV);
    DCFlushRange(sBufferDRC, sBufferSizeDRC);

    // -------------------------------------------------------------------------
    // Flip buffers (swap front and back)
    // -------------------------------------------------------------------------
    // This makes our newly-drawn back buffer become the displayed front
    // buffer, and the old front buffer becomes the new back buffer for
    // the next frame.
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void DrawText(int col, int row, const char* text)
{
    if (!sInitialized || !text) {
        return;
    }

    // OSScreenPutFontEx draws text at the specified character position.
    // The coordinate system is character-based, not pixel-based.
    //
    // Reference: https://wut.devkitpro.org/group__coreinit__screen.html
    OSScreenPutFontEx(SCREEN_TV, col, row, text);
    OSScreenPutFontEx(SCREEN_DRC, col, row, text);
}

void DrawTextTV(int col, int row, const char* text)
{
    if (!sInitialized || !text) {
        return;
    }
    OSScreenPutFontEx(SCREEN_TV, col, row, text);
}

void DrawTextDRC(int col, int row, const char* text)
{
    if (!sInitialized || !text) {
        return;
    }
    OSScreenPutFontEx(SCREEN_DRC, col, row, text);
}

void DrawTextF(int col, int row, const char* fmt, ...)
{
    if (!sInitialized || !fmt) {
        return;
    }

    // Format the string into a temporary buffer
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Draw the formatted text
    DrawText(col, row, buffer);
}

void* GetTVBuffer()
{
    return sBufferTV;
}

void* GetDRCBuffer()
{
    return sBufferDRC;
}

uint32_t GetTVBufferSize()
{
    return sBufferSizeTV;
}

uint32_t GetDRCBufferSize()
{
    return sBufferSizeDRC;
}

} // namespace Screen
