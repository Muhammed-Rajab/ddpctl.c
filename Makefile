# compiler & flags
CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -O0 -g
CFLAGS  += -Iinclude -Ilib/gifdec
CFLAGS  += -fsanitize=address,undefined
LDFLAGS := -lm
LDFLAGS += -fsanitize=address,undefined

# directories
SRC_DIR   := src
LIB_DIR   := lib/gifdec
BUILD_DIR := build

# source files
APP_SRC   := ddpctl.c
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
LIB_SRC   := $(LIB_DIR)/gifdec.c

OBJ_FILES := \
	$(BUILD_DIR)/ddpctl.o \
	$(SRC_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
	$(BUILD_DIR)/gifdec.o

TARGET := ddpctl

# rules
all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@ $(LDFLAGS)

$(BUILD_DIR)/ddpctl.o: $(APP_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gifdec.o: $(LIB_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

