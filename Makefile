# Architecture detection
ifeq ($(ARCH),32)
    SUFFIX = 
    HDE = hde32
    TARGET = i686-w64-mingw32
else
    SUFFIX = 64
    HDE = hde64
    TARGET = x86_64-w64-mingw32
endif

# Compiler settings
CXX = clang++
ifdef CROSS
    CXXFLAGS = -shared -static -O3 -Oz -fuse-ld=lld --target=$(TARGET)
else
    CXXFLAGS = -shared -static -O3 -Oz
endif
INCLUDES = -I src -I src/minhook/include

# Output files
EMU_DLL = emu.upc_r1_loader$(SUFFIX).dll
ASI_FILE = uplay_asi$(SUFFIX).asi
STANDALONE = uplay_r1_loader$(SUFFIX).dll

# Source files
EMU_SOURCES = src/dllmain.cpp src/pch.cpp src/uplay_data.cpp src/logging.cpp src/steam_impl.cpp
ASI_SOURCES = src/uplay_hook.cpp \
              src/minhook/src/hook.c \
              src/minhook/src/buffer.c \
              src/minhook/src/trampoline.c \
              src/minhook/src/hde/$(HDE).c

# Libraries
EMU_LIBS = -lkernel32 -luser32 -ladvapi32 -lshell32
ASI_LIBS = -lkernel32 -luser32

# Default target
all: $(EMU_DLL) $(ASI_FILE) $(STANDALONE)

# Build emulator DLL
$(EMU_DLL): $(EMU_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(EMU_LIBS)

# Build ASI hook
$(ASI_FILE): $(ASI_SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(ASI_LIBS)

# Create standalone DLL (copy of emulator with different name)
$(STANDALONE): $(EMU_DLL)
	cp $< $@

# Clean build artifacts
clean:
	rm -f $(EMU_DLL) $(ASI_FILE) $(STANDALONE)

# Build both architectures
all64:
	$(MAKE) ARCH=64

all32:
	$(MAKE) ARCH=32

.PHONY: all clean all64 all32
