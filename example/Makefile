CC = clang
CFLAGS = -Wall -Wextra -std=c11 -O3 -I../vendor/chaitea/src
LDFLAGS = -L../vendor/chaitea/bin
LIBS = -lchai

TARGET = example
SRC_DIR = .
BUILD_DIR = .
OBJ_DIR = $(BUILD_DIR)/obj

SRCS = $(SRC_DIR)/example.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(BUILD_DIR)/$(TARGET)

# Link the final binary (passes library search path and library flag here)
$(BUILD_DIR)/$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

# Compile source files into object files (includes -I header paths here)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directories if they don't exist
$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)/$(TARGET)

.PHONY: all clean
