# ASCII Preview Tool Makefile
#
# Builds a desktop preview tool for visualizing the Wii U menu layout.
# Uses the ACTUAL rendering code from menu.cpp via menu_render.cpp.

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I.

# Output
TARGET := preview_demo

# Sources - using actual menu rendering code
SOURCES := preview_demo.cpp menu_render.cpp
OBJECTS := $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Dependencies
preview_demo.o: preview_demo.cpp menu_render.h stubs/renderer_stub.h stubs/settings_stub.h stubs/categories_stub.h
menu_render.o: menu_render.cpp menu_render.h stubs/renderer_stub.h stubs/titles_stub.h stubs/settings_stub.h stubs/categories_stub.h stubs/image_loader_stub.h stubs/buttons_stub.h stubs/text_input_stub.h

clean:
	rm -f $(TARGET) $(OBJECTS)

run: $(TARGET)
	./$(TARGET)

run-nocolor: $(TARGET)
	./$(TARGET) --no-color

# Show with line numbers enabled
run-numbers: $(TARGET)
	./$(TARGET) --no-color --numbers

# Show favorites category only
run-favs: $(TARGET)
	./$(TARGET) --no-color --category 1

.PHONY: all clean run run-nocolor run-numbers run-favs
