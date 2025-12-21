#pragma once

namespace MiiverseTest {

// Run a basic connectivity test
// Returns true if all tests pass
// Logs results via notifications
bool runBasicTest();

// Test acquiring service token only
bool testAcquireToken();

// Test fetching posts for a known title (e.g., Mario Kart 8)
bool testFetchPosts();

} // namespace MiiverseTest
