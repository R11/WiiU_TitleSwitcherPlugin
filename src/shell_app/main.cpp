/**
 * Title Switcher standalone app entry point.
 *
 * Boots directly into the menu, lets the user pick a title, and dispatches via
 * SYSLaunchTitle. When the launched title eventually exits, Aroma's autoboot
 * mechanism brings the user back here.
 *
 * Designed to be configured as the default boot target via AutobootModule.
 */

#include "menu/menu.h"
#include "menu/categories.h"
#include "render/renderer.h"
#include "render/image_loader.h"
#include "titles/titles.h"
#include "storage/settings.h"
#include "presets/title_presets.h"

#include <whb/proc.h>
#include <sysapp/launch.h>
#include <coreinit/debug.h>

#include <cstdint>
#include <cstdio>

namespace Menu { namespace Hooks {

void ReportError(const char* message)
{
    OSReport("[TitleSwitcher] %s\n", message);
}

}}

int main(int /*argc*/, char** /*argv*/)
{
    WHBProcInit();

    Settings::Init();
    Settings::Load();

    Menu::Init();
    ImageLoader::Init();
    Titles::Load();
    TitlePresets::Load();

    if (!Renderer::Init()) {
        OSReport("[TitleSwitcher] Renderer::Init() failed\n");
        ImageLoader::Shutdown();
        Menu::Shutdown();
        WHBProcShutdown();
        return -1;
    }

    Menu::InitForWebPreview();

    uint64_t titleToLaunch = 0;

    while (WHBProcIsRunning()) {
        Menu::FrameResult result = Menu::HandleInputFrame();
        Menu::RenderFrame();
        Renderer::EndFrame();

        if (!result.shouldContinue) {
            titleToLaunch = result.titleToLaunch;
            break;
        }
    }

    Settings::Save();

    Renderer::Shutdown();
    ImageLoader::Shutdown();
    Menu::Shutdown();

    if (titleToLaunch != 0) {
        SYSLaunchTitle(titleToLaunch);
    }

    WHBProcShutdown();
    return 0;
}
