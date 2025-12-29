/**
 * Screenshot Capture Module
 *
 * Captures the game's framebuffer when the menu opens,
 * allowing it to be displayed as a background or thumbnail.
 */

#pragma once

#include "../render/renderer.h"

namespace Screenshot {

/**
 * Capture the current game framebuffer.
 * Must be called BEFORE Renderer::Init() as OSScreen overwrites the display.
 *
 * @return true if capture succeeded
 */
bool CaptureFromGame();

/**
 * Get the captured TV screenshot as an image handle.
 * Returns nullptr if no screenshot was captured.
 */
Renderer::ImageHandle GetTVImage();

/**
 * Get the captured DRC screenshot as an image handle.
 * Returns nullptr if no screenshot was captured.
 */
Renderer::ImageHandle GetDRCImage();

/**
 * Check if a screenshot is currently available.
 */
bool HasCapture();

/**
 * Get the captured image dimensions.
 */
int GetCaptureWidth();
int GetCaptureHeight();

/**
 * Release the captured screenshot memory.
 * Called automatically when menu closes.
 */
void Release();

}
