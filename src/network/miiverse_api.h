#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MiiverseAPI {

// ParamPack structure - sent as X-Nintendo-ParamPack header (base64 encoded)
// Format: backslash-delimited key\value pairs
// Example decoded: \title_id\0005000010101D00\platform_id\1\region_id\2\...
struct ParamPack {
    uint64_t titleId;
    uint32_t accessKey;
    uint8_t platformId;      // 0=Wii U, 1=3DS
    uint8_t regionId;        // 1=JPN, 2=USA, 4=EUR
    uint8_t languageId;      // 0=JPN, 1=ENG, etc.
    uint8_t countryId;       // 49=USA, 110=etc.
    uint8_t areaId;
    uint8_t networkRestriction;
    uint8_t friendRestriction;
    uint8_t ratingRestriction;
    uint8_t ratingOrganization;
    uint64_t transferableId;
    std::string tzName;      // e.g., "America/New_York"
    int32_t utcOffset;       // in seconds, e.g., -14400
    uint8_t remasterVersion;

    std::string encode() const;
    static ParamPack createForTitle(uint64_t titleId);
};

// Pretendo/Nintendo Miiverse API endpoints
// When connected to Pretendo, these resolve to pretendo.cc
namespace Endpoints {
    constexpr const char* DISCOVERY = "https://discovery.olv.pretendo.cc/v1/endpoint";
    constexpr const char* API_BASE = "https://api.olv.pretendo.cc";

    // GET /v1/communities/{community_id}/posts
    // GET /v1/posts?title_id={title_id}
    // POST /v1/posts
}

// Required HTTP headers for Miiverse API calls
namespace Headers {
    constexpr const char* SERVICE_TOKEN = "X-Nintendo-ServiceToken";
    constexpr const char* PARAM_PACK = "X-Nintendo-ParamPack";
    constexpr const char* USER_AGENT = "WiiU/POLV-4.0.0";  // Miiverse app version
}

// Post data structure (parsed from XML response)
struct Post {
    std::string id;
    std::string body;
    std::string screenName;     // author username
    std::string miiDataBase64;  // Mii face data
    std::string paintingUrl;    // drawing URL if present
    std::string createdAt;      // timestamp
    uint32_t empathyCount;      // "Yeah!" count
    uint32_t replyCount;
    uint64_t titleId;
    bool isSpoiler;
};

// Result of API calls
struct ApiResult {
    bool success;
    int httpCode;
    std::string errorMessage;
    std::string rawResponse;
};

// Initialize the API (call once)
bool init();

// Cleanup
void shutdown();

// Acquire service token from console's authenticated PNID session
// Returns empty string on failure
std::string acquireServiceToken();

// Fetch posts for a specific title
ApiResult fetchPostsForTitle(uint64_t titleId, const std::string& serviceToken);

// Fetch posts from a community
ApiResult fetchCommunityPosts(uint32_t communityId, const std::string& serviceToken);

// Parse XML response into Post structures
std::vector<Post> parsePostsXml(const std::string& xml);

} // namespace MiiverseAPI
