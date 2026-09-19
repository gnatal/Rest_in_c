# -----------------------------------------------------------------------------
# Rest_in_c - REST API powered by CExpress
# -----------------------------------------------------------------------------

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    CC ?= gcc
    PLATFORM_CFLAGS = -D_GNU_SOURCE
    OPENSSL_CFLAGS ?= $(shell pkg-config --cflags openssl 2>/dev/null)
    OPENSSL_LIBS   ?= $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
else
    # macOS: Prefer gcc-16 (from Homebrew, matches c_server and avoids macOS SDK tapi issues)
    ifneq ($(shell which gcc-16 2>/dev/null),)
        CC = gcc-16
    else
        CC = gcc
    endif
    PLATFORM_CFLAGS =
    OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null || echo /opt/homebrew/opt/openssl@3)
    ifeq ($(shell test -d $(OPENSSL_PREFIX)/include && echo yes),yes)
        OPENSSL_CFLAGS = -I$(OPENSSL_PREFIX)/include
        OPENSSL_LIBS   = -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto
    else
        OPENSSL_CFLAGS = $(shell pkg-config --cflags openssl 2>/dev/null)
        OPENSSL_LIBS   = $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
    endif
endif

CEXPRESS_DIR = vendor/cexpress
LIB_CEXPRESS = $(CEXPRESS_DIR)/build/lib/libcexpress.a

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin

SRCS = $(shell find src -name '*.c' 2>/dev/null)
OBJS = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CFLAGS  = -Wall -Wextra -std=c11 -O2 -I$(CEXPRESS_DIR)/lib -Isrc $(PLATFORM_CFLAGS) $(OPENSSL_CFLAGS)
LDFLAGS = $(LIB_CEXPRESS) $(OPENSSL_LIBS) -lpthread

TARGET = $(BIN_DIR)/rest_api

.PHONY: all run clean help test

all: $(TARGET)
	@ln -sf $(TARGET) rest_api

# Ensure CExpress engine library is compiled
$(LIB_CEXPRESS):
	@echo "==> Building CExpress engine..."
	$(MAKE) -C $(CEXPRESS_DIR)

# Generic object compilation with automatic header dependencies
$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Link target binary
$(TARGET): $(OBJS) $(LIB_CEXPRESS) | $(BIN_DIR)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

-include $(shell find $(OBJ_DIR) -name '*.d' 2>/dev/null)

PORT ?= 8080

run: all
	@echo "==> Starting REST API on port $(PORT)..."
	PORT=$(PORT) ./rest_api

test: all
	@if [ -f ./test_api.sh ]; then ./test_api.sh; else echo "No test_api.sh script found"; fi

clean:
	@echo "==> Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) rest_api

help:
	@echo "Available make targets:"
	@echo "  make         - Build the rest_api executable"
	@echo "  make run     - Build and run the server (PORT=8080 by default)"
	@echo "  make test    - Run automated test suite"
	@echo "  make clean   - Remove build artifacts"
