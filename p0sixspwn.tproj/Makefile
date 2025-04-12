# Output binary name
TARGET = inject

# Source file
SRC = inject.c

# Minimum macOS version
MACOS_MIN_VERSION = 10.11

# Compiler and general flags
CC = gcc
CFLAGS = -std=gnu99 -Wall -I. -mmacosx-version-min=$(MACOS_MIN_VERSION)

# Frameworks to link
FRAMEWORKS = -framework MobileDevice -framework CoreFoundation

# Detect current macOS version
MACOS_VER := $(shell sw_vers -productVersion | cut -d '.' -f 1,2)

# Framework path depending on macOS version
ifeq ($(shell [ $(MACOS_VER) \< 10.15 ] && echo true),true)
    FRAMEWORK_PATH = /System/Library/PrivateFrameworks
    ARCHS = x86_64
else
    FRAMEWORK_PATH = /Library/Apple/System/Library/PrivateFrameworks
    ARCHS = x86_64 arm64
endif

# Combined -F and framework flags
LDFLAGS = -F$(FRAMEWORK_PATH) $(FRAMEWORKS)

# Temp binaries for each arch
BINARIES = $(addprefix $(TARGET)-, $(ARCHS))

# Default build target
all: $(TARGET)

# Per-arch compilation rule
$(TARGET)-%: $(SRC)
	$(CC) $(CFLAGS) -arch $* -o $@ $< $(LDFLAGS)

# Create fat binary (or thin if only 1 arch)
$(TARGET): $(BINARIES)
ifeq ($(words $(ARCHS)),1)
	mv $< $@
else
	lipo -create -output $@ $^
endif

# Clean up
clean:
	rm -f $(TARGET) $(BINARIES)
