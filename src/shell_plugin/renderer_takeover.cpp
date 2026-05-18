/**
 * Plugin-side renderer takeover hooks.
 *
 * The plugin overlays the menu on top of a running app, so it must:
 *  - capture the foreground app's GX2 framebuffers (used as a fallback when
 *    MEMAllocFromMappedMemoryForGX2Ex can't satisfy the request in
 *    memory-constrained games like Ducktales, Shovel Knight)
 *  - save/restore Display Controller registers so the app can resume cleanly
 *  - suppress the HOME button menu while our overlay is active
 *
 * The app shell provides no-op versions of these hooks in renderer_app_init.cpp.
 */

#include "../core/render/renderer.h"
#include "utils/dc.h"

#include <wups.h>
#include <coreinit/screen.h>
#include <coreinit/systeminfo.h>
#include <gx2/surface.h>

namespace {

struct StoredBuffer {
    void* buffer = nullptr;
    uint32_t buffer_size = 0;
    int32_t mode = 0;
    GX2SurfaceFormat surface_format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    GX2BufferingMode buffering_mode = GX2_BUFFERING_MODE_DOUBLE;
};

StoredBuffer gStoredTVBuffer;
StoredBuffer gStoredDRCBuffer;

bool gHomeButtonWasEnabled = false;
DCRegisters gSavedDCRegisters;

}

DECL_FUNCTION(void, GX2SetTVBuffer, void *buffer, uint32_t buffer_size,
              int32_t tv_render_mode, GX2SurfaceFormat format,
              GX2BufferingMode buffering_mode) {
    gStoredTVBuffer.buffer = buffer;
    gStoredTVBuffer.buffer_size = buffer_size;
    gStoredTVBuffer.mode = tv_render_mode;
    gStoredTVBuffer.surface_format = format;
    gStoredTVBuffer.buffering_mode = buffering_mode;
    real_GX2SetTVBuffer(buffer, buffer_size, tv_render_mode, format, buffering_mode);
}
WUPS_MUST_REPLACE(GX2SetTVBuffer, WUPS_LOADER_LIBRARY_GX2, GX2SetTVBuffer);

DECL_FUNCTION(void, GX2SetDRCBuffer, void *buffer, uint32_t buffer_size,
              uint32_t drc_mode, GX2SurfaceFormat surface_format,
              GX2BufferingMode buffering_mode) {
    gStoredDRCBuffer.buffer = buffer;
    gStoredDRCBuffer.buffer_size = buffer_size;
    gStoredDRCBuffer.mode = drc_mode;
    gStoredDRCBuffer.surface_format = surface_format;
    gStoredDRCBuffer.buffering_mode = buffering_mode;
    real_GX2SetDRCBuffer(buffer, buffer_size, drc_mode, surface_format, buffering_mode);
}
WUPS_MUST_REPLACE(GX2SetDRCBuffer, WUPS_LOADER_LIBRARY_GX2, GX2SetDRCBuffer);

namespace Renderer { namespace Hooks {

void* GetCapturedTVBuffer(uint32_t* outSize)
{
    if (outSize) *outSize = gStoredTVBuffer.buffer_size;
    return gStoredTVBuffer.buffer;
}

void* GetCapturedDRCBuffer(uint32_t* outSize)
{
    if (outSize) *outSize = gStoredDRCBuffer.buffer_size;
    return gStoredDRCBuffer.buffer;
}

void OnBeforeInit()
{
    gHomeButtonWasEnabled = OSIsHomeButtonMenuEnabled();
    DCSaveRegisters(&gSavedDCRegisters);
}

void OnAfterInit()
{
    OSEnableHomeButtonMenu(false);
}

void OnInitFailed()
{
    DCRestoreRegisters(&gSavedDCRegisters);
}

void OnShutdown()
{
    OSEnableHomeButtonMenu(gHomeButtonWasEnabled);
    DCRestoreRegisters(&gSavedDCRegisters);
}

}}
