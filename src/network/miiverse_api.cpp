#include "miiverse_api.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>

// For base64 encoding
#include <cstdlib>

#ifdef __WIIU__
// WUT headers
#include <nn/act.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>

// libcurlwrapper - must have CURLWrapperModule loaded
#include <curl/curl.h>

// nn_act functions not exposed in WUT headers - manually declare
// These symbols exist in nn_act.rpl (see cafe/nn_act.def)
extern "C" {
    // AcquireIndependentServiceToken(char* tokenOut, const char* clientId)
    // clientId for Miiverse/OLV service
    int AcquireIndependentServiceToken__Q2_2nn3actFPcPCc(char* tokenOut, const char* clientId);
}

#endif // __WIIU__

namespace MiiverseAPI {

namespace {

// Base64 encoding table
constexpr char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    uint32_t val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        output.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (output.size() % 4) {
        output.push_back('=');
    }
    return output;
}

// CURL write callback
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

#ifdef __WIIU__
bool curlInitialized = false;
#endif

} // anonymous namespace

std::string ParamPack::encode() const {
    std::ostringstream ss;

    // Format: \key\value\key\value...
    // Title ID as decimal (not hex) for ParamPack
    ss << "\\title_id\\" << titleId;
    ss << "\\access_key\\" << accessKey;
    ss << "\\platform_id\\" << static_cast<int>(platformId);
    ss << "\\region_id\\" << static_cast<int>(regionId);
    ss << "\\language_id\\" << static_cast<int>(languageId);
    ss << "\\country_id\\" << static_cast<int>(countryId);
    ss << "\\area_id\\" << static_cast<int>(areaId);
    ss << "\\network_restriction\\" << static_cast<int>(networkRestriction);
    ss << "\\friend_restriction\\" << static_cast<int>(friendRestriction);
    ss << "\\rating_restriction\\" << static_cast<int>(ratingRestriction);
    ss << "\\rating_organization\\" << static_cast<int>(ratingOrganization);
    ss << "\\transferable_id\\" << transferableId;
    ss << "\\tz_name\\" << tzName;
    ss << "\\utc_offset\\" << utcOffset;
    ss << "\\remaster_version\\" << static_cast<int>(remasterVersion);
    ss << "\\";

    return base64Encode(ss.str());
}

ParamPack ParamPack::createForTitle(uint64_t titleId) {
    ParamPack pack{};
    pack.titleId = titleId;
    pack.accessKey = 0;
    pack.platformId = 0;       // Wii U
    pack.regionId = 2;         // USA (adjust based on console)
    pack.languageId = 1;       // English
    pack.countryId = 49;       // USA
    pack.areaId = 0;
    pack.networkRestriction = 0;
    pack.friendRestriction = 0;
    pack.ratingRestriction = 20;
    pack.ratingOrganization = 0;
    pack.transferableId = 0;
    pack.tzName = "America/New_York";
    pack.utcOffset = -18000;   // EST
    pack.remasterVersion = 0;
    return pack;
}

bool init() {
#ifdef __WIIU__
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        // CURLWrapperModule not loaded
        return false;
    }
    curlInitialized = true;
    return true;
#else
    return false;
#endif
}

void shutdown() {
#ifdef __WIIU__
    if (curlInitialized) {
        curl_global_cleanup();
        curlInitialized = false;
    }
#endif
}

std::string acquireServiceToken() {
#ifdef __WIIU__
    // Initialize nn_act if needed
    nn::act::Initialize();

    char tokenBuffer[512] = {0};

    // Try to acquire service token for OLV (Miiverse) service
    // The client ID might need to be determined - trying common values
    // Nintendo's OLV client ID was reportedly: "87cd32617f1985439ea608c2571571fe"
    // Pretendo may use the same or different
    int result = AcquireIndependentServiceToken__Q2_2nn3actFPcPCc(
        tokenBuffer,
        "87cd32617f1985439ea608c2571571fe"  // OLV client ID
    );

    if (result < 0) {
        // Try without client ID or with different ID
        // Some services use empty string
        result = AcquireIndependentServiceToken__Q2_2nn3actFPcPCc(tokenBuffer, "");
    }

    nn::act::Finalize();

    if (result >= 0 && tokenBuffer[0] != '\0') {
        return std::string(tokenBuffer);
    }

    return "";
#else
    return "";
#endif
}

ApiResult fetchPostsForTitle(uint64_t titleId, const std::string& serviceToken) {
    ApiResult result{};
    result.success = false;

#ifdef __WIIU__
    if (!curlInitialized) {
        result.errorMessage = "CURL not initialized";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.errorMessage = "Failed to init CURL handle";
        return result;
    }

    // Build URL - fetch posts filtered by title_id
    std::ostringstream urlStream;
    urlStream << Endpoints::API_BASE << "/v1/posts?title_id=";
    urlStream << std::hex << std::setfill('0') << std::setw(16) << titleId;
    std::string url = urlStream.str();

    // Build ParamPack
    ParamPack paramPack = ParamPack::createForTitle(titleId);
    std::string encodedParamPack = paramPack.encode();

    // Set up headers
    struct curl_slist* headers = nullptr;

    std::string tokenHeader = std::string(Headers::SERVICE_TOKEN) + ": " + serviceToken;
    headers = curl_slist_append(headers, tokenHeader.c_str());

    std::string paramPackHeader = std::string(Headers::PARAM_PACK) + ": " + encodedParamPack;
    headers = curl_slist_append(headers, paramPackHeader.c_str());

    std::string userAgentHeader = std::string("User-Agent: ") + Headers::USER_AGENT;
    headers = curl_slist_append(headers, userAgentHeader.c_str());

    headers = curl_slist_append(headers, "Accept: application/xml");

    // Configure request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.rawResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.httpCode);
        result.success = (result.httpCode >= 200 && result.httpCode < 300);
        if (!result.success) {
            result.errorMessage = "HTTP " + std::to_string(result.httpCode);
        }
    } else {
        result.errorMessage = curl_easy_strerror(res);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
#else
    result.errorMessage = "Not running on Wii U";
    return result;
#endif
}

ApiResult fetchCommunityPosts(uint32_t communityId, const std::string& serviceToken) {
    ApiResult result{};
    result.success = false;

#ifdef __WIIU__
    if (!curlInitialized) {
        result.errorMessage = "CURL not initialized";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.errorMessage = "Failed to init CURL handle";
        return result;
    }

    // Build URL
    std::ostringstream urlStream;
    urlStream << Endpoints::API_BASE << "/v1/communities/" << communityId << "/posts";
    std::string url = urlStream.str();

    // Use a generic ParamPack (community browsing, not title-specific)
    ParamPack paramPack = ParamPack::createForTitle(0);
    std::string encodedParamPack = paramPack.encode();

    // Set up headers
    struct curl_slist* headers = nullptr;

    std::string tokenHeader = std::string(Headers::SERVICE_TOKEN) + ": " + serviceToken;
    headers = curl_slist_append(headers, tokenHeader.c_str());

    std::string paramPackHeader = std::string(Headers::PARAM_PACK) + ": " + encodedParamPack;
    headers = curl_slist_append(headers, paramPackHeader.c_str());

    std::string userAgentHeader = std::string("User-Agent: ") + Headers::USER_AGENT;
    headers = curl_slist_append(headers, userAgentHeader.c_str());

    headers = curl_slist_append(headers, "Accept: application/xml");

    // Configure request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.rawResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.httpCode);
        result.success = (result.httpCode >= 200 && result.httpCode < 300);
        if (!result.success) {
            result.errorMessage = "HTTP " + std::to_string(result.httpCode);
        }
    } else {
        result.errorMessage = curl_easy_strerror(res);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
#else
    result.errorMessage = "Not running on Wii U";
    return result;
#endif
}

std::vector<Post> parsePostsXml(const std::string& xml) {
    std::vector<Post> posts;

    // Simple XML parsing - find <post> elements
    // In production, use a proper XML parser
    size_t pos = 0;
    while ((pos = xml.find("<post>", pos)) != std::string::npos) {
        Post post{};

        size_t endPos = xml.find("</post>", pos);
        if (endPos == std::string::npos) break;

        std::string postXml = xml.substr(pos, endPos - pos + 7);

        // Extract fields (simplified - real impl should handle CDATA, entities, etc.)
        auto extractField = [&postXml](const std::string& tag) -> std::string {
            std::string openTag = "<" + tag + ">";
            std::string closeTag = "</" + tag + ">";
            size_t start = postXml.find(openTag);
            if (start == std::string::npos) return "";
            start += openTag.length();
            size_t end = postXml.find(closeTag, start);
            if (end == std::string::npos) return "";
            return postXml.substr(start, end - start);
        };

        post.id = extractField("id");
        post.body = extractField("body");
        post.screenName = extractField("screen_name");
        post.createdAt = extractField("created_at");
        post.paintingUrl = extractField("painting_url");

        std::string empathy = extractField("empathy_count");
        post.empathyCount = empathy.empty() ? 0 : std::stoul(empathy);

        std::string replies = extractField("reply_count");
        post.replyCount = replies.empty() ? 0 : std::stoul(replies);

        post.isSpoiler = extractField("is_spoiler") == "1";

        posts.push_back(std::move(post));
        pos = endPos + 7;
    }

    return posts;
}

} // namespace MiiverseAPI
