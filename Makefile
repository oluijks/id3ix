# Default to clang, but respect a compiler the user actually chose, from either
# the environment or the command line. A plain `CC = clang` would override an
# environment CC, and `CC ?= clang` would never apply at all, since make
# predefines CC and ?= only assigns when a variable is unset.
ifeq ($(origin CC),default)
CC = clang
endif

VERSION = 0.1.0
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -DID3IX_VERSION=\"$(VERSION)\"
PREFIX ?= /usr/local
TARGET = id3ix
SOURCE = src/main.c
HEADERS = $(wildcard src/*.h)

.PHONY: all clean test format format-check lint install uninstall

all: $(TARGET)

$(TARGET): $(SOURCE) $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	VERSION=$(VERSION) ./tests/run.sh

format:
	clang-format -i src/*.c $(HEADERS)

format-check:
	clang-format --dry-run --Werror src/*.c $(HEADERS)

lint:
	cppcheck --enable=warning,style,performance,portability --error-exitcode=1 src/*.c $(HEADERS)
	shellcheck tests/run.sh

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
