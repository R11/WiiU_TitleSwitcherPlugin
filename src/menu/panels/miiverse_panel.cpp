/**
 * Miiverse Panel Implementation
 * Displays Miiverse API test results from Pretendo.
 */

#include "miiverse_panel.h"
#include "../menu_state.h"
#include "../menu.h"
#include "../../render/renderer.h"
#include "../../input/buttons.h"
#include "../../storage/settings.h"

#ifdef __WIIU__
#include "../../network/miiverse_api.h"
#include <notifications/notifications.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace Menu {
namespace MiiversePanel {

using namespace Internal;

namespace {

// Test state
enum class TestState {
    IDLE,
    INITIALIZING,
    ACQUIRING_TOKEN,
    FETCHING_POSTS,
    SUCCESS,
    ERROR
};

TestState sState = TestState::IDLE;
std::string sStatusMessage = "Press A to run Miiverse API test";
std::string sErrorMessage;
std::string sTokenPreview;
int sPostCount = 0;

#ifdef __WIIU__
std::vector<MiiverseAPI::Post> sPosts;
#endif

// Colors
constexpr uint32_t COLOR_SUCCESS = 0xA6E3A1FF;  // Green
constexpr uint32_t COLOR_ERROR = 0xF38BA8FF;    // Red
constexpr uint32_t COLOR_PENDING = 0xF9E2AFFF;  // Yellow
constexpr uint32_t COLOR_INFO = 0x89B4FAFF;     // Blue

uint32_t getStateColor()
{
    switch (sState) {
        case TestState::SUCCESS: return COLOR_SUCCESS;
        case TestState::ERROR: return COLOR_ERROR;
        case TestState::INITIALIZING:
        case TestState::ACQUIRING_TOKEN:
        case TestState::FETCHING_POSTS:
            return COLOR_PENDING;
        default:
            return COLOR_INFO;
    }
}

const char* getStateLabel()
{
    switch (sState) {
        case TestState::IDLE: return "READY";
        case TestState::INITIALIZING: return "INIT...";
        case TestState::ACQUIRING_TOKEN: return "TOKEN...";
        case TestState::FETCHING_POSTS: return "FETCH...";
        case TestState::SUCCESS: return "SUCCESS";
        case TestState::ERROR: return "ERROR";
        default: return "???";
    }
}

}  // anonymous namespace

void Reset()
{
    sState = TestState::IDLE;
    sStatusMessage = "Press A to run Miiverse API test";
    sErrorMessage.clear();
    sTokenPreview.clear();
    sPostCount = 0;
#ifdef __WIIU__
    sPosts.clear();
#endif
}

void RunTest()
{
#ifdef __WIIU__
    Reset();
    sState = TestState::INITIALIZING;
    sStatusMessage = "Initializing API...";

    // Step 1: Initialize
    if (!MiiverseAPI::init()) {
        sState = TestState::ERROR;
        sErrorMessage = "Failed to init (CURLWrapperModule loaded?)";
        sStatusMessage = "Initialization failed";
        return;
    }

    // Step 2: Acquire token
    sState = TestState::ACQUIRING_TOKEN;
    sStatusMessage = "Acquiring service token...";

    std::string token = MiiverseAPI::acquireServiceToken();
    if (token.empty()) {
        sState = TestState::ERROR;
        sErrorMessage = "Failed to acquire service token";
        sStatusMessage = "Token acquisition failed";
        MiiverseAPI::shutdown();
        return;
    }

    // Show token preview
    if (token.length() > 16) {
        sTokenPreview = token.substr(0, 8) + "..." + token.substr(token.length() - 8);
    } else {
        sTokenPreview = token;
    }

    // Step 3: Fetch posts
    sState = TestState::FETCHING_POSTS;
    sStatusMessage = "Fetching posts...";

    // Try Mario Kart 8 (common game with likely posts)
    constexpr uint64_t MK8_TITLE_ID = 0x0005000010101D00ULL;
    MiiverseAPI::ApiResult result = MiiverseAPI::fetchPostsForTitle(MK8_TITLE_ID, token);

    if (!result.success) {
        sState = TestState::ERROR;
        sErrorMessage = result.errorMessage;
        if (result.httpCode > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), " (HTTP %d)", result.httpCode);
            sErrorMessage += buf;
        }
        sStatusMessage = "Fetch failed";
        MiiverseAPI::shutdown();
        return;
    }

    // Step 4: Parse response
    sPosts = MiiverseAPI::parsePostsXml(result.rawResponse);
    sPostCount = static_cast<int>(sPosts.size());

    sState = TestState::SUCCESS;
    char msg[64];
    snprintf(msg, sizeof(msg), "Found %d posts!", sPostCount);
    sStatusMessage = msg;

    MiiverseAPI::shutdown();
#else
    // Non-Wii U: show mock result
    sState = TestState::ERROR;
    sErrorMessage = "Not running on Wii U hardware";
    sStatusMessage = "Test unavailable";
#endif
}

void Render()
{
    auto& settings = Settings::Get();

    // Header
    Renderer::DrawText(0, CATEGORY_ROW, "MIIVERSE API TEST", settings.categoryColor);
    drawHeaderDivider();

    // Status box
    int row = LIST_START_ROW;

    Renderer::DrawText(LIST_START_COL, row, "Status:", settings.headerColor);
    Renderer::DrawText(LIST_START_COL + 10, row, getStateLabel(), getStateColor());
    row++;

    Renderer::DrawText(LIST_START_COL, row, sStatusMessage.c_str(), settings.titleColor);
    row += 2;

    // Token info
    if (!sTokenPreview.empty()) {
        Renderer::DrawText(LIST_START_COL, row, "Token:", settings.headerColor);
        Renderer::DrawText(LIST_START_COL + 10, row, sTokenPreview.c_str(), COLOR_SUCCESS);
        row++;
    }

    // Error message
    if (!sErrorMessage.empty()) {
        Renderer::DrawText(LIST_START_COL, row, "Error:", settings.headerColor);
        row++;
        Renderer::DrawText(LIST_START_COL + 2, row, sErrorMessage.c_str(), COLOR_ERROR);
        row++;
    }

    // Post count
    if (sPostCount > 0) {
        row++;
        Renderer::DrawText(LIST_START_COL, row, "Posts found:", settings.headerColor);
        Renderer::DrawTextF(LIST_START_COL + 14, row, COLOR_SUCCESS, "%d", sPostCount);
        row += 2;

#ifdef __WIIU__
        // Show first few posts
        Renderer::DrawText(LIST_START_COL, row, "Sample posts:", settings.headerColor);
        row++;

        int maxShow = 3;
        for (int i = 0; i < std::min(maxShow, sPostCount); i++) {
            const auto& post = sPosts[i];

            // Author
            std::string author = post.screenName.empty() ? "(anonymous)" : post.screenName;
            Renderer::DrawTextF(LIST_START_COL + 2, row, settings.highlightedTitleColor,
                               "%d. %s", i + 1, author.c_str());
            row++;

            // Body preview (truncate to fit)
            if (!post.body.empty()) {
                std::string body = post.body;
                if (body.length() > 50) {
                    body = body.substr(0, 47) + "...";
                }
                Renderer::DrawText(LIST_START_COL + 5, row, body.c_str(), settings.titleColor);
                row++;
            }

            // Empathy count
            if (post.empathyCount > 0) {
                Renderer::DrawTextF(LIST_START_COL + 5, row, 0xF9E2AFFF,
                                   "Yeah! x%d", post.empathyCount);
                row++;
            }
        }
#endif
    }

    // Instructions at bottom
    int bottomRow = Renderer::GetGridHeight() - 2;
    Renderer::DrawText(LIST_START_COL, bottomRow, "[A:Run Test]  [B:Back]", 0x888888FF);
}

void HandleInput(uint32_t pressed)
{
    if (Buttons::Actions::CANCEL.Pressed(pressed)) {
        sCurrentMode = Mode::SETTINGS;
        return;
    }

    if (Buttons::Actions::CONFIRM.Pressed(pressed)) {
        if (sState == TestState::IDLE || sState == TestState::SUCCESS || sState == TestState::ERROR) {
            RunTest();
        }
    }
}

}  // namespace MiiversePanel
}  // namespace Menu
