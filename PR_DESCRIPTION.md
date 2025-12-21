# Miiverse/Pretendo Integration - Minimal Prototype

## Summary

Adds a minimal prototype for fetching Miiverse posts from Pretendo Network directly on the Wii U console.

- Implements `MiiverseAPI` namespace with token acquisition and post fetching
- Uses `libcurlwrapper` for HTTPS requests (no external server needed)
- Includes test harness for validating connectivity
- Documents ParamPack format and API endpoints

## Files Added

| File | Purpose |
|------|---------|
| `src/network/miiverse_api.h` | API declarations, ParamPack/Post structs |
| `src/network/miiverse_api.cpp` | Implementation with libcurlwrapper |
| `src/network/miiverse_test.h` | Test function declarations |
| `src/network/miiverse_test.cpp` | Test harness for token + fetch |
| `src/network/README.md` | Full documentation |

## Test Plan

### Prerequisites

1. **Wii U connected to Pretendo Network** with working PNID
2. **CURLWrapperModule installed**: Copy `CURLWrapperModule.wms` to:
   ```
   sd:/wiiu/environments/aroma/modules/
   ```
3. **Build with libcurlwrapper**: Add to Makefile when integrating:
   ```makefile
   LIBS := -lcurlwrapper ...(existing libs)
   SOURCES := ... src/network
   ```

### Testing Steps

1. Build and deploy the plugin to Wii U
2. Trigger `MiiverseTest::runBasicTest()` (e.g., via menu action or button combo)
3. Watch notifications for results:
   - `"Token acquired: XXXX...XXXX"` = token acquisition works
   - `"Found N posts"` = API fetch works
   - `"First post by: Username"` = parsing works

### Expected Results

| Test | Success | Failure |
|------|---------|---------|
| Init | "API initialized OK" | "Failed to init (CURLWrapperModule loaded?)" |
| Token | "Token acquired: ..." | "Failed to acquire service token" |
| Fetch | "Found N posts" | "Fetch failed: HTTP XXX" or error message |

### Known Unknowns (Need Hardware Validation)

- [ ] OLV client ID (`87cd32617f1985439ea608c2571571fe`) - may need Pretendo-specific value
- [ ] `AcquireIndependentServiceToken` works from plugin context
- [ ] Pretendo API accepts console-generated tokens
- [ ] Response XML format matches expectations

## Technical Details

### Authentication Flow

```
Console PNID Session
    ↓
AcquireIndependentServiceToken("87cd32617f1985439ea608c2571571fe")
    ↓
X-Nintendo-ServiceToken header
    ↓
GET https://api.olv.pretendo.cc/v1/posts?title_id=XXX
```

### ParamPack Format

Base64-encoded backslash-delimited string:
```
\title_id\{DECIMAL}\platform_id\0\region_id\2\language_id\1\country_id\49\...
```

## Next Steps (After Validation)

1. Integrate into plugin menu as "Community Posts" section
2. Add UI for displaying posts with Mii avatars
3. Cache results to minimize network calls
4. Handle offline/error states gracefully
