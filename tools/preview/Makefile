# ASCII Preview Tool Makefile
#
# Builds a desktop preview tool for visualizing the Wii U menu layout.

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

# Output
TARGET := preview_demo

# Sources
SOURCES := preview_demo.cpp ascii_renderer.cpp
OBJECTS := $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Dependencies
preview_demo.o: preview_demo.cpp ascii_renderer.h
ascii_renderer.o: ascii_renderer.cpp ascii_renderer.h

clean:
	rm -f $(TARGET) $(OBJECTS)

run: $(TARGET)
	./$(TARGET)

run-nocolor: $(TARGET)
	./$(TARGET) --no-color

run-ascii: $(TARGET)
	./$(TARGET) --ascii --no-color

.PHONY: all clean run run-nocolor run-ascii
