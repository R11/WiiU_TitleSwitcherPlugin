# Test Makefile for TitleSwitcherPlugin
# Builds and runs unit tests using Google Test

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DTEST_BUILD

# Include paths (mocks first to override real implementations)
INCLUDES = -Imocks -I../src

# Check if bundled gtest is built, use it; otherwise use system gtest
BUNDLED_GTEST_LIB := ../extern/googletest/build/lib/libgtest.a
ifeq ($(wildcard $(BUNDLED_GTEST_LIB)),)
    # System gtest (Ubuntu package)
    GTEST_LIB = -lgtest -lgtest_main -pthread
else
    # Bundled gtest
    INCLUDES += -I../extern/googletest/googletest/include
    GTEST_LIB = -L../extern/googletest/build/lib -lgtest -lgtest_main -pthread
endif

# Source files to test (compiled with test mocks)
TEST_SRCS = \
	unit/buttons_test.cpp \
	unit/settings_test.cpp

# Source files to compile (with test mocks)
SRC_SRCS = \
	../src/storage/settings.cpp

# Mock implementations
MOCK_SRCS = \
	mocks/ui/layout_mock.cpp

TARGET = run_tests

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(TEST_SRCS) $(SRC_SRCS) $(MOCK_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(GTEST_LIB)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
