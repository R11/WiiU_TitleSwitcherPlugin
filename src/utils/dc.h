/**
 * Display Controller (DC) Register Access
 *
 * This file provides low-level access to the Wii U's display controller registers.
 * These registers control how framebuffer data is displayed on the TV and GamePad screens.
 *
 * ATTRIBUTION:
 * This code is derived from WiiUPluginLoaderBackend's DrawUtils implementation.
 * Original source: https://github.com/wiiu-env/WiiUPluginLoaderBackend
 * Original author: Maschell
 * License: GPLv3 (https://www.gnu.org/licenses/gpl-3.0.en.html)
 *
 * WHY THIS IS NEEDED:
 * When we take over the screen with OSScreen, the game's graphics system is still
 * running and may try to update display settings. By saving and restoring these
 * registers, we ensure the game's display returns to exactly the state it was in
 * before we opened our menu.
 *
 * Without this, you'll see:
 * - Garbled/corrupted graphics when returning to the game
 * - Wrong resolution or color format
 * - Screen tearing or flickering
 *
 * TECHNICAL DETAILS:
 * The Wii U uses an AMD Radeon-based GPU (codenamed "Latte"). The display controller
 * registers are memory-mapped at physical address 0x0C200000. Each screen (TV and DRC)
 * has its own set of registers, offset by 0x800 bytes (0x200 uint32_t values).
 *
 * The __OSPhysicalToEffectiveUncached() function converts the physical address to
 * a virtual address we can read/write from our code, with caching disabled to ensure
 * we always see the current hardware state.
 *
 * REGISTER DESCRIPTIONS:
 * - D1GRPH_ENABLE (0x1840): Controls whether the graphics layer is enabled
 * - D1GRPH_CONTROL (0x1841): Format, color depth, and other display settings
 * - D1GRPH_PITCH (0x1848): Number of bytes per scanline (stride)
 * - D1OVL_PITCH (0x1866): Overlay layer pitch (used for video overlays)
 *
 * REFERENCE:
 * - AMD GPU documentation (similar register layout to desktop GPUs)
 * - WiiUBrew wiki: https://wiiubrew.org/wiki/Hardware/Latte
 */

#pragma once

#include <coreinit/debug.h>
#include <coreinit/screen.h>

// -----------------------------------------------------------------------------
// External function declaration
// -----------------------------------------------------------------------------
// This function is provided by coreinit but not declared in public headers.
// It converts a physical memory address to an uncached virtual address.
//
// Parameters:
//   physicalAddress - Physical address in the range the GPU can access
//
// Returns:
//   Virtual address that can be used by CPU code, with caching disabled
// -----------------------------------------------------------------------------
extern "C" uint32_t __OSPhysicalToEffectiveUncached(uint32_t physicalAddress);

// -----------------------------------------------------------------------------
// Display Controller Register Indices
// -----------------------------------------------------------------------------
// These are offsets into the DC register space (in uint32_t units, not bytes).
// Each screen (TV=0, DRC=1) has its own register bank offset by 0x200.
// -----------------------------------------------------------------------------

// Graphics layer enable register
// Bit 0: Enable/disable the graphics plane
constexpr uint32_t D1GRPH_ENABLE_REG  = 0x1840;

// Graphics control register
// Contains format, color depth, byte ordering, and other settings
constexpr uint32_t D1GRPH_CONTROL_REG = 0x1841;

// Graphics pitch register
// Number of bytes per horizontal line (width * bytes_per_pixel, aligned)
constexpr uint32_t D1GRPH_PITCH_REG   = 0x1848;

// Overlay pitch register
// Pitch for the overlay layer (used for video playback, etc.)
constexpr uint32_t D1OVL_PITCH_REG    = 0x1866;

// -----------------------------------------------------------------------------
// Base address for display controller registers
// -----------------------------------------------------------------------------
// Physical address where DC registers are memory-mapped
constexpr uint32_t DC_REGISTER_BASE = 0x0C200000;

// Offset between screen register banks (TV vs DRC)
// Each screen's registers are 0x800 bytes (0x200 uint32_t) apart
constexpr uint32_t DC_SCREEN_OFFSET = 0x200;

// -----------------------------------------------------------------------------
// Register Access Functions
// -----------------------------------------------------------------------------

/**
 * Read a 32-bit value from a display controller register.
 *
 * @param screen Which screen's registers to access (SCREEN_TV or SCREEN_DRC)
 * @param index  Register index (e.g., D1GRPH_CONTROL_REG)
 * @return       Current value of the register, or 0 if in ECO mode
 *
 * Example:
 *   uint32_t tvControl = DCReadReg32(SCREEN_TV, D1GRPH_CONTROL_REG);
 */
inline uint32_t DCReadReg32(const OSScreenID screen, const uint32_t index)
{
    // ECO mode is a low-power state where the display controller may not be
    // accessible. Attempting to read registers in this state could hang.
    if (OSIsECOMode()) {
        return 0;
    }

    // Get uncached pointer to register space
    // Using uncached access ensures we read the actual hardware state,
    // not a stale cached value
    const auto regs = reinterpret_cast<uint32_t*>(
        __OSPhysicalToEffectiveUncached(DC_REGISTER_BASE)
    );

    // Calculate register offset for this screen
    // TV (screen 0) uses base offset, DRC (screen 1) adds DC_SCREEN_OFFSET
    return regs[index + (screen * DC_SCREEN_OFFSET)];
}

/**
 * Write a 32-bit value to a display controller register.
 *
 * @param screen Which screen's registers to modify (SCREEN_TV or SCREEN_DRC)
 * @param index  Register index (e.g., D1GRPH_CONTROL_REG)
 * @param value  Value to write to the register
 *
 * WARNING: Writing incorrect values can cause display corruption or system hangs.
 * Only write values that were previously read from the same register.
 *
 * Example:
 *   // Restore previously saved control register
 *   DCWriteReg32(SCREEN_TV, D1GRPH_CONTROL_REG, savedTvControl);
 */
inline void DCWriteReg32(const OSScreenID screen, const uint32_t index, const uint32_t value)
{
    // Don't access registers in ECO mode
    if (OSIsECOMode()) {
        return;
    }

    // Get uncached pointer to register space
    const auto regs = reinterpret_cast<uint32_t*>(
        __OSPhysicalToEffectiveUncached(DC_REGISTER_BASE)
    );

    // Write to the appropriate register for this screen
    regs[index + (screen * DC_SCREEN_OFFSET)] = value;
}

// -----------------------------------------------------------------------------
// Convenience structure for saving/restoring all relevant registers
// -----------------------------------------------------------------------------

/**
 * Structure to hold all DC registers we need to save/restore.
 * Use this to save state before taking over the screen and restore after.
 */
struct DCRegisters {
    // TV screen registers
    uint32_t tvEnable;
    uint32_t tvControl;
    uint32_t tvPitch;
    uint32_t tvOverlayPitch;

    // DRC (GamePad) screen registers
    uint32_t drcEnable;
    uint32_t drcControl;
    uint32_t drcPitch;
    uint32_t drcOverlayPitch;
};

/**
 * Save all display controller registers for both screens.
 *
 * Call this BEFORE initializing OSScreen to capture the game's display state.
 *
 * @param outRegs Pointer to structure that will receive the saved values
 *
 * Example:
 *   DCRegisters savedRegs;
 *   DCSaveRegisters(&savedRegs);
 *   // ... do menu stuff ...
 *   DCRestoreRegisters(&savedRegs);
 */
inline void DCSaveRegisters(DCRegisters* outRegs)
{
    if (!outRegs) return;

    outRegs->tvEnable       = DCReadReg32(SCREEN_TV, D1GRPH_ENABLE_REG);
    outRegs->tvControl      = DCReadReg32(SCREEN_TV, D1GRPH_CONTROL_REG);
    outRegs->tvPitch        = DCReadReg32(SCREEN_TV, D1GRPH_PITCH_REG);
    outRegs->tvOverlayPitch = DCReadReg32(SCREEN_TV, D1OVL_PITCH_REG);

    outRegs->drcEnable       = DCReadReg32(SCREEN_DRC, D1GRPH_ENABLE_REG);
    outRegs->drcControl      = DCReadReg32(SCREEN_DRC, D1GRPH_CONTROL_REG);
    outRegs->drcPitch        = DCReadReg32(SCREEN_DRC, D1GRPH_PITCH_REG);
    outRegs->drcOverlayPitch = DCReadReg32(SCREEN_DRC, D1OVL_PITCH_REG);
}

/**
 * Restore all display controller registers for both screens.
 *
 * Call this AFTER you're done with OSScreen to return to the game's display state.
 *
 * @param regs Pointer to structure containing previously saved values
 *
 * Example:
 *   DCRestoreRegisters(&savedRegs);  // Game's display is now restored
 */
inline void DCRestoreRegisters(const DCRegisters* regs)
{
    if (!regs) return;

    DCWriteReg32(SCREEN_TV, D1GRPH_ENABLE_REG, regs->tvEnable);
    DCWriteReg32(SCREEN_TV, D1GRPH_CONTROL_REG, regs->tvControl);
    DCWriteReg32(SCREEN_TV, D1GRPH_PITCH_REG, regs->tvPitch);
    DCWriteReg32(SCREEN_TV, D1OVL_PITCH_REG, regs->tvOverlayPitch);

    DCWriteReg32(SCREEN_DRC, D1GRPH_ENABLE_REG, regs->drcEnable);
    DCWriteReg32(SCREEN_DRC, D1GRPH_CONTROL_REG, regs->drcControl);
    DCWriteReg32(SCREEN_DRC, D1GRPH_PITCH_REG, regs->drcPitch);
    DCWriteReg32(SCREEN_DRC, D1OVL_PITCH_REG, regs->drcOverlayPitch);
}
