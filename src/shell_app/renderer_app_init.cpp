/**
 * App-side renderer hooks (no-ops).
 *
 * The standalone app owns the display for its lifetime — there's no host app
 * whose state we need to preserve. All takeover hooks are stubs.
 */

#include "render/renderer.h"

namespace Renderer { namespace Hooks {

void* GetCapturedTVBuffer(uint32_t* outSize)
{
    if (outSize) *outSize = 0;
    return nullptr;
}

void* GetCapturedDRCBuffer(uint32_t* outSize)
{
    if (outSize) *outSize = 0;
    return nullptr;
}

void OnBeforeInit() {}
void OnAfterInit() {}
void OnInitFailed() {}
void OnShutdown() {}

}}
