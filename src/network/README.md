# Miiverse API Prototype

Minimal prototype for fetching Miiverse posts from Pretendo Network.

## Files

- `miiverse_api.h` - API declarations and data structures
- `miiverse_api.cpp` - Implementation using libcurlwrapper
- `miiverse_test.h/cpp` - Test harness

## Dependencies Required

Add to Makefile `LIBS`:
```makefile
LIBS := -lcurlwrapper ... (existing libs)
```

Requires **CURLWrapperModule** to be installed:
- Copy `CURLWrapperModule.wms` to `sd:/wiiu/environments/aroma/modules/`

## ParamPack Format

The `X-Nintendo-ParamPack` header is a base64-encoded backslash-delimited string:

```
\title_id\{TITLE_ID_DECIMAL}\access_key\0\platform_id\0\region_id\2\language_id\1\country_id\49\area_id\0\network_restriction\0\friend_restriction\0\rating_restriction\20\rating_organization\0\transferable_id\0\tz_name\America/New_York\utc_offset\-18000\remaster_version\0\
```

| Field | Description | Example |
|-------|-------------|---------|
| title_id | Title ID as decimal | 1407375153044736 |
| platform_id | 0=Wii U, 1=3DS | 0 |
| region_id | 1=JPN, 2=USA, 4=EUR | 2 |
| language_id | 0=JPN, 1=ENG, etc. | 1 |
| country_id | Country code | 49 (USA) |
| tz_name | Timezone string | America/New_York |
| utc_offset | UTC offset in seconds | -18000 |

## API Endpoints

Pretendo replaces `*.olv.nintendo.net` with `*.olv.pretendo.cc`:

| Endpoint | URL |
|----------|-----|
| Discovery | `https://discovery.olv.pretendo.cc/v1/endpoint` |
| Posts by title | `https://api.olv.pretendo.cc/v1/posts?title_id={hex}` |
| Community posts | `https://api.olv.pretendo.cc/v1/communities/{id}/posts` |

## Required Headers

| Header | Value |
|--------|-------|
| X-Nintendo-ServiceToken | Token from `AcquireIndependentServiceToken` |
| X-Nintendo-ParamPack | Base64-encoded ParamPack |
| User-Agent | `WiiU/POLV-4.0.0` |
| Accept | `application/xml` |

## Service Token Acquisition

The token is acquired using undocumented nn_act function:

```cpp
extern "C" {
    int AcquireIndependentServiceToken__Q2_2nn3actFPcPCc(
        char* tokenOut,
        const char* clientId
    );
}
```

The OLV (Miiverse) client ID is: `87cd32617f1985439ea608c2571571fe`

**Note**: This is the Nintendo client ID. Pretendo may accept this or require their own. Testing needed.

## Usage

```cpp
#include "network/miiverse_api.h"
#include "network/miiverse_test.h"

// Run all tests
MiiverseTest::runBasicTest();

// Or manual usage:
MiiverseAPI::init();
std::string token = MiiverseAPI::acquireServiceToken();
auto result = MiiverseAPI::fetchPostsForTitle(0x0005000010101D00ULL, token);
auto posts = MiiverseAPI::parsePostsXml(result.rawResponse);
MiiverseAPI::shutdown();
```

## Known Unknowns

1. **Client ID**: The OLV client ID may need to be different for Pretendo
2. **Token format**: Service token structure from Pretendo vs Nintendo may differ
3. **API compatibility**: Pretendo's API may have subtle differences from Nintendo's
4. **Rate limiting**: Unknown if Pretendo rate-limits API requests

## Testing Steps

1. Ensure Wii U is connected to Pretendo Network
2. Ensure CURLWrapperModule is installed
3. Build and deploy plugin
4. Call `MiiverseTest::runBasicTest()` (e.g., from a menu action)
5. Check notifications for results

## Response Format

Posts are returned as XML:

```xml
<result>
  <has_error>0</has_error>
  <posts>
    <post>
      <id>AYYGAABB1WTJ...</id>
      <title_id>0005000010101D00</title_id>
      <screen_name>Username</screen_name>
      <body>Post text here</body>
      <painting format="tga">BASE64...</painting>
      <mii>BASE64_MII_DATA</mii>
      <created_at>2024-12-21 17:11:01</created_at>
      <empathy_count>5</empathy_count>
      <reply_count>2</reply_count>
      <is_spoiler>0</is_spoiler>
    </post>
  </posts>
</result>
```

## References

- [Inkay source](https://github.com/PretendoNetwork/Inkay) - Pretendo URL patches
- [miiverse-api](https://github.com/PretendoNetwork/miiverse-api) - Pretendo server
- [NintendoClients](https://github.com/kinnay/NintendoClients) - API documentation
- [libcurlwrapper](https://github.com/wiiu-env/libcurlwrapper) - HTTP for Wii U
