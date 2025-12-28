# Makefile for Mini Uplay API Emulator
# Cross-compile for Windows x86 from Linux

# Cross-compiler settings
CC = i686-w64-mingw32-gcc
CXX = i686-w64-mingw32-g++
WINDRES = i686-w64-mingw32-windres
STRIP = i686-w64-mingw32-strip

# Project settings
PROJECT_NAME = upc_r1_loader
TARGET = $(PROJECT_NAME).dll
SRC_DIR = src
BUILD_DIR = build
DIST_DIR = dist
DIST_TARGET = $(DIST_DIR)/$(TARGET)

# Source files
SOURCES = $(SRC_DIR)/dllmain.cpp \
          $(SRC_DIR)/pch.cpp \
          $(SRC_DIR)/uplay_data.cpp

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Compiler flags
CXXFLAGS = -m32 \
           -std=c++11 \
           -Wall \
           -Wextra \
           -O2 \
           -DWIN32 \
           -D_WINDOWS \
           -D_USRDLL \
           -DNDEBUG \
           -I$(SRC_DIR)

# Linker flags
LDFLAGS = -m32 \
          -shared \
          -static-libgcc \
          -static-libstdc++ \
          -Wl,--subsystem,windows \
          -Wl,--enable-stdcall-fixup \
          -Wl,--kill-at

# Libraries to link
LIBS = -lkernel32 -luser32 -ladvapi32

# Default target
all: $(DIST_TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create dist directory
$(DIST_DIR):
	mkdir -p $(DIST_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link the DLL
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(STRIP) $@
	@echo "Build complete: $(TARGET)"

# Copy to dist directory
$(DIST_TARGET): $(TARGET) | $(DIST_DIR)
	mv $(TARGET) $(DIST_TARGET)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(DIST_DIR)
	rm -f $(TARGET)

# Install cross-compiler (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install gcc-mingw-w64-i686 g++-mingw-w64-i686

# Debug build
debug: CXXFLAGS += -g -DDEBUG -O0
debug: CXXFLAGS := $(filter-out -O2 -DNDEBUG,$(CXXFLAGS))
debug: $(DIST_TARGET)

# Show variables (for debugging makefile)
show:
	@echo "CC: $(CC)"
	@echo "CXX: $(CXX)"
	@echo "STRIP: $(STRIP)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"

# Check if cross-compiler is installed
check-deps:
	@which $(CXX) > /dev/null || (echo "Error: MinGW-w64 cross-compiler not found. Run 'make install-deps' first." && exit 1)
	@echo "Cross-compiler found: $(CXX)"

.PHONY: all clean install-deps debug show check-deps
