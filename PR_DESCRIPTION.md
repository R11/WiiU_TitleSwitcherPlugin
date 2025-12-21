# Miiverse/Pretendo Integration - Minimal Prototype

## Summary

Adds a minimal prototype for fetching Miiverse posts from Pretendo Network directly on the Wii U console.

- Implements `MiiverseAPI` namespace with token acquisition and post fetching
- Uses `libcurlwrapper` for HTTPS requests (no external server needed)
- **Adds test panel accessible from Settings menu**
- Documents ParamPack format and API endpoints

## Files Added/Modified

| File | Purpose |
|------|---------|
| `src/network/miiverse_api.h` | API declarations, ParamPack/Post structs |
| `src/network/miiverse_api.cpp` | Implementation with libcurlwrapper |
| `src/network/miiverse_test.h` | Test function declarations |
| `src/network/miiverse_test.cpp` | Test harness for token + fetch |
| `src/network/README.md` | Full documentation |
| `src/menu/panels/miiverse_panel.h` | Test panel header |
| `src/menu/panels/miiverse_panel.cpp` | Test panel UI implementation |
| `src/menu/menu.h` | Added MIIVERSE_TEST mode |
| `src/menu/menu.cpp` | Panel dispatch for new mode |
| `src/menu/menu_state.h` | Added ACTION_MIIVERSE_TEST |
| `src/menu/panels/settings_panel.cpp` | Menu entry handler |
| `Makefile` | Added src/network, libcurlwrapper |

## Test Plan

### Prerequisites

1. **Wii U connected to Pretendo Network** with working PNID
2. **CURLWrapperModule installed**: Copy `CURLWrapperModule.wms` to:
   ```
   sd:/wiiu/environments/aroma/modules/
   ```

### Testing Steps

1. Build and deploy the plugin to Wii U
2. Open Title Switcher menu (L + R + Minus)
3. Go to **Settings** (press Y)
4. Select **"Miiverse Test"** from the list
5. Press **A** to run the API test
6. View results on screen:
   - Status indicator (READY → INIT → TOKEN → FETCH → SUCCESS/ERROR)
   - Token preview if acquired
   - Post count and sample posts on success
   - Error message on failure

### Expected Results

| Status | What It Means |
|--------|---------------|
| `READY` | Press A to start test |
| `INIT...` | Initializing libcurlwrapper |
| `TOKEN...` | Acquiring service token from console |
| `FETCH...` | Making HTTPS request to Pretendo |
| `SUCCESS` | Posts retrieved and parsed |
| `ERROR` | Something failed (see error message) |

### Possible Error Messages

| Error | Cause |
|-------|-------|
| "Failed to init (CURLWrapperModule loaded?)" | Module not installed |
| "Failed to acquire service token" | PNID not logged in, or wrong client ID |
| "Fetch failed: HTTP 401" | Token rejected by Pretendo |
| "Fetch failed: HTTP 404" | No posts for this title/community |
| "Not running on Wii U hardware" | Test run in non-Wii U environment |

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

### Test Panel UI

```
┌─────────────────────────────────────────┐
│ MIIVERSE API TEST                       │
│ ─────────────────────────               │
│                                         │
│ Status:   SUCCESS                       │
│ Found 5 posts!                          │
│                                         │
│ Token:    ABCD1234...XYZ98765           │
│                                         │
│ Posts found: 5                          │
│                                         │
│ Sample posts:                           │
│   1. PlayerName                         │
│      This is a post about Mario Kart!   │
│      Yeah! x3                           │
│   2. AnotherUser                        │
│      Great game!                        │
│                                         │
│ [A:Run Test]  [B:Back]                  │
└─────────────────────────────────────────┘
```

## Next Steps (After Validation)

1. Integrate into plugin menu as "Community Posts" section
2. Add UI for displaying posts with Mii avatars
3. Cache results to minimize network calls
4. Handle offline/error states gracefully
