CC = clang
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
