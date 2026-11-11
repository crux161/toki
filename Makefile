CC = clang
CXX = clang++
CFLAGS = -Wall -Wextra -std=c11 -O3 -D_POSIX_C_SOURCE=200809L -Ivendor/chaitea/src -Ivendor/chaitea/vendor_win $(MINIFB_INCLUDES)
LDFLAGS = -Lvendor/chaitea/bin -L$(BUILD_DIR)
LIBS = -lchai -lminifb -lc++ $(MINIFB_FRAMEWORKS)

TARGET = toki
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

SRCS = $(SRC_DIR)/toki.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LIB_CHAI = vendor/chaitea/bin/libchai.a

# minifb, macOS Metal backend. Kept out of $(OBJ_DIR) so these pattern rules
# cannot be confused with the ones for src/ above.
MINIFB_DIR = vendor/minifb
MINIFB_OBJ_DIR = $(BUILD_DIR)/minifb
LIB_MINIFB = $(BUILD_DIR)/libminifb.a
MINIFB_INCLUDES = -I$(MINIFB_DIR)/include
MINIFB_FRAMEWORKS = -framework Cocoa -framework Carbon -framework QuartzCore \
	-framework Metal -framework MetalKit

# Mirrors minifb's own CMakeLists.txt: -std=gnu11 rather than the c11 used for
# src/, because the Cocoa headers want GNU extensions. The SSE2/NEON/AVX2 files
# select themselves from the target arch in MiniFB_stretch_internal.h, so an
# x86-only file compiles to nothing here and needs no special flags.
MINIFB_CFLAGS = -std=gnu11 -O2 -Wall -Wextra -Wno-switch -Wno-unused-function \
	-Wno-unused-parameter -Wno-implicit-fallthrough \
	-DUSE_METAL_API -I$(MINIFB_DIR)/include -I$(MINIFB_DIR)/src
MINIFB_CXXFLAGS = -std=gnu++11 -O2 -Wall -Wextra -Wno-switch -Wno-unused-function \
	-Wno-unused-parameter -Wno-implicit-fallthrough \
	-DUSE_METAL_API -I$(MINIFB_DIR)/include -I$(MINIFB_DIR)/src

# Paths are relative to $(MINIFB_DIR)/src. MiniFB_cpp.cpp is not optional even
# though src/ is C: every backend calls release_cpp_stub() on window teardown,
# and that symbol is defined only there. It is what drags -lc++ into $(LIBS).
MINIFB_C_SRCS = MiniFB_common.c MiniFB_cpu_x86.c MiniFB_internal.c MiniFB_stretch.c \
	MiniFB_stretch_sse2.c MiniFB_stretch_neon.c MiniFB_stretch_avx2.c \
	MiniFB_timer.c MiniFB_utf8.c
MINIFB_CXX_SRCS = MiniFB_cpp.cpp
MINIFB_M_SRCS = macos/MacMiniFB.m macos/OSXWindow.m macos/OSXView.m \
	macos/OSXViewDelegate.m

MINIFB_OBJS = $(MINIFB_C_SRCS:%.c=$(MINIFB_OBJ_DIR)/%.o) \
	$(MINIFB_CXX_SRCS:%.cpp=$(MINIFB_OBJ_DIR)/%.o) \
	$(MINIFB_M_SRCS:%.m=$(MINIFB_OBJ_DIR)/%.o)

# Default target is now 'all'
all: $(BUILD_DIR)/$(TARGET)

# Automatically build the chaitea submodule library if it doesn't exist
$(LIB_CHAI):
	$(MAKE) -C vendor/chaitea CFLAGS="-std=gnu11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -Ivendor_win"

# Link the final binary, ensuring libchai.a and libminifb.a are built first
$(BUILD_DIR)/$(TARGET): $(OBJS) $(LIB_CHAI) $(LIB_MINIFB) | $(BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

# Compile minifb's C, C++ and Objective-C sources into object files
$(MINIFB_OBJ_DIR)/%.o: $(MINIFB_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(MINIFB_CFLAGS) -c $< -o $@

$(MINIFB_OBJ_DIR)/%.o: $(MINIFB_DIR)/src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(MINIFB_CXXFLAGS) -c $< -o $@

$(MINIFB_OBJ_DIR)/%.o: $(MINIFB_DIR)/src/%.m
	@mkdir -p $(dir $@)
	$(CC) $(MINIFB_CFLAGS) -c $< -o $@

$(LIB_MINIFB): $(MINIFB_OBJS) | $(BUILD_DIR)
	ar rcs $@ $(MINIFB_OBJS)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directories if they don't exist
$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

symbols: $(LIB_CHAI)
	nm -u $(LIB_CHAI) || nm $(LIB_CHAI)

clean:
	rm -rf $(BUILD_DIR)

clean-all: clean
	$(MAKE) -C vendor/chaitea clean
inspect:
	grep -A 20 "typedef struct" vendor/chaitea/src/chaitea/terminal.h

.PHONY: all clean clean-all symbols inspect
