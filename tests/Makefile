# Test Makefile for TitleSwitcherPlugin
# Builds and runs unit tests using Google Test

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -DTEST_BUILD

# Include paths
INCLUDES = -I../src -Imocks

# Google Test libraries (Ubuntu package)
GTEST_LIB = -lgtest -lgtest_main -pthread

# Source files to test (compiled with test mocks)
TEST_SRCS = \
	unit/buttons_test.cpp \
	unit/settings_test.cpp

# We need to compile settings.cpp with our mocks
SRC_SRCS = \
	../src/storage/settings.cpp

TARGET = run_tests

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(TEST_SRCS) $(SRC_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(GTEST_LIB)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
