#include "miiverse_test.h"
#include "miiverse_api.h"

#ifdef __WIIU__
#include <notifications/notifications.h>
#endif

#include <string>

namespace MiiverseTest {

namespace {

void log(const char* message) {
#ifdef __WIIU__
    NotificationModule_AddInfoNotification(message);
#else
    (void)message;
#endif
}

void logError(const char* message) {
#ifdef __WIIU__
    NotificationModule_AddErrorNotification(message);
#else
    (void)message;
#endif
}

} // anonymous namespace

bool testAcquireToken() {
    log("Testing service token acquisition...");

    std::string token = MiiverseAPI::acquireServiceToken();

    if (token.empty()) {
        logError("Failed to acquire service token");
        return false;
    }

    // Log first/last few chars for verification without exposing full token
    std::string preview = token.substr(0, 8) + "..." + token.substr(token.length() - 8);
    std::string msg = "Token acquired: " + preview;
    log(msg.c_str());

    return true;
}

bool testFetchPosts() {
    log("Testing post fetch...");

    std::string token = MiiverseAPI::acquireServiceToken();
    if (token.empty()) {
        logError("Cannot test fetch: no token");
        return false;
    }

    // Test with Mario Kart 8 title ID (common game, likely has posts)
    // MK8 US: 0005000010101D00
    constexpr uint64_t MARIO_KART_8_TITLE_ID = 0x0005000010101D00ULL;

    log("Fetching MK8 posts...");
    MiiverseAPI::ApiResult result = MiiverseAPI::fetchPostsForTitle(
        MARIO_KART_8_TITLE_ID, token
    );

    if (!result.success) {
        std::string errMsg = "Fetch failed: " + result.errorMessage;
        logError(errMsg.c_str());

        // Log HTTP code if we got one
        if (result.httpCode > 0) {
            std::string codeMsg = "HTTP code: " + std::to_string(result.httpCode);
            log(codeMsg.c_str());
        }

        // Log response snippet for debugging
        if (!result.rawResponse.empty()) {
            std::string snippet = result.rawResponse.substr(0, 100);
            std::string respMsg = "Response: " + snippet;
            log(respMsg.c_str());
        }

        return false;
    }

    // Parse response
    auto posts = MiiverseAPI::parsePostsXml(result.rawResponse);

    std::string countMsg = "Found " + std::to_string(posts.size()) + " posts";
    log(countMsg.c_str());

    // Log first post if any
    if (!posts.empty()) {
        const auto& first = posts[0];
        std::string postMsg = "First post by: " + first.screenName;
        log(postMsg.c_str());

        if (!first.body.empty()) {
            std::string bodyPreview = first.body.substr(0, 50);
            std::string bodyMsg = "Body: " + bodyPreview;
            log(bodyMsg.c_str());
        }
    }

    return true;
}

bool runBasicTest() {
    log("=== Miiverse API Test ===");

    // Initialize
    if (!MiiverseAPI::init()) {
        logError("Failed to init MiiverseAPI (CURLWrapperModule loaded?)");
        return false;
    }

    log("API initialized OK");

    bool allPassed = true;

    // Test 1: Token acquisition
    if (!testAcquireToken()) {
        allPassed = false;
    }

    // Test 2: Fetch posts
    if (!testFetchPosts()) {
        allPassed = false;
    }

    // Cleanup
    MiiverseAPI::shutdown();

    if (allPassed) {
        log("=== All tests PASSED ===");
    } else {
        logError("=== Some tests FAILED ===");
    }

    return allPassed;
}

} // namespace MiiverseTest
