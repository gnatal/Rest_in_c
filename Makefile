UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    CC = gcc
    CFLAGS = -Wall -Wextra -std=c11 -O2 -Ivendor/cexpress/lib -D_GNU_SOURCE
    LDFLAGS = -Lvendor/cexpress/build/lib -lcexpress -luring
else
    CC = gcc-16
    CFLAGS = -Wall -Wextra -std=c11 -O2 -Ivendor/cexpress/lib
    LDFLAGS = -Lvendor/cexpress/build/lib -lcexpress
endif

SQLITE_PREFIX ?= $(shell brew --prefix sqlite3 2>/dev/null || brew --prefix sqlite 2>/dev/null || echo /opt/homebrew/opt/sqlite)
ifeq ($(shell test -d $(SQLITE_PREFIX)/include && echo yes),yes)
    SQLITE_CFLAGS = -I$(SQLITE_PREFIX)/include
    SQLITE_LDFLAGS = -L$(SQLITE_PREFIX)/lib -lsqlite3
else ifeq ($(shell pkg-config --exists sqlite3 2>/dev/null && echo yes),yes)
    SQLITE_CFLAGS = $(shell pkg-config --cflags sqlite3)
    SQLITE_LDFLAGS = $(shell pkg-config --libs sqlite3)
else
    SQLITE_CFLAGS =
    SQLITE_LDFLAGS = -lsqlite3
endif

OPENSSL_PREFIX ?= $(shell brew --prefix openssl@3 2>/dev/null || brew --prefix openssl@1.1 2>/dev/null || echo /opt/homebrew/opt/openssl)
ifeq ($(shell test -d $(OPENSSL_PREFIX)/include && echo yes),yes)
    OPENSSL_CFLAGS = -I$(OPENSSL_PREFIX)/include
    OPENSSL_LDFLAGS = -L$(OPENSSL_PREFIX)/lib -lcrypto
else ifeq ($(shell pkg-config --exists libcrypto 2>/dev/null && echo yes),yes)
    OPENSSL_CFLAGS = $(shell pkg-config --cflags libcrypto)
    OPENSSL_LDFLAGS = $(shell pkg-config --libs libcrypto)
else
    OPENSSL_CFLAGS =
    OPENSSL_LDFLAGS = -lcrypto
endif

CFLAGS += $(SQLITE_CFLAGS) $(OPENSSL_CFLAGS)
LDFLAGS += $(SQLITE_LDFLAGS) $(OPENSSL_LDFLAGS)

SRCS = main.c db.c crypto.c controllers_user.c
OBJS = $(SRCS:.c=.o)
TARGET = realworld_api

.PHONY: all clean run cexpress

all: $(TARGET)

cexpress:
	$(MAKE) -C vendor/cexpress

$(TARGET): cexpress $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
	$(MAKE) -C vendor/cexpress clean

run: $(TARGET)
	./$(TARGET)
