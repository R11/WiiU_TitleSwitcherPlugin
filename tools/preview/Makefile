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

# Snapshot directory
SNAPSHOT_DIR := snapshots

# Screen types for snapshots
SCREENS := drc tv1080 tv720 tv480

# Generate all snapshots (for updating golden files)
# Creates snapshots for all 4 screen types
snapshots: $(TARGET)
	@mkdir -p $(SNAPSHOT_DIR)
	@echo "Generating snapshots for all screen types..."
	@for screen in $(SCREENS); do \
		echo "  $$screen..."; \
		./$(TARGET) --no-color --screen $$screen > $(SNAPSHOT_DIR)/$${screen}_default.txt; \
	done
	@echo "Snapshots generated in $(SNAPSHOT_DIR)/"

# Verify snapshots match current output (for CI/testing)
verify-snapshots: $(TARGET)
	@echo "Verifying snapshots..."
	@failed=0; \
	for screen in $(SCREENS); do \
		if ./$(TARGET) --no-color --screen $$screen | diff -q - $(SNAPSHOT_DIR)/$${screen}_default.txt > /dev/null 2>&1; then \
			echo "  $$screen: OK"; \
		else \
			echo "  $$screen: MISMATCH"; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 1 ]; then \
		echo "Snapshot verification FAILED"; \
		exit 1; \
	else \
		echo "All snapshots match!"; \
	fi

# Show diff if snapshots don't match
diff-snapshots: $(TARGET)
	@for screen in $(SCREENS); do \
		echo "=== $$screen ==="; \
		./$(TARGET) --no-color --screen $$screen | diff - $(SNAPSHOT_DIR)/$${screen}_default.txt || true; \
	done

.PHONY: all clean run run-nocolor run-numbers run-favs snapshots verify-snapshots diff-snapshots
