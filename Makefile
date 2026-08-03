# CC is deliberately not set here. Make defaults it to cc, the system compiler,
# and leaving it alone lets the environment or `make CC=...` choose one. CI
# pins the compilers it tests explicitly.
VERSION = 0.1.0
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -DID3IX_VERSION=\"$(VERSION)\"
PREFIX ?= /usr/local
TARGET = id3ix
DEBUG_TARGET = $(TARGET)-debug
SOURCE = src/main.c
HEADERS = $(wildcard src/*.h)

# Sanitizer build flags. -fno-sanitize-recover=all is the important one:
# UndefinedBehaviorSanitizer otherwise prints its diagnostic and carries on with
# a zero exit status, so a test run would pass despite having detected
# undefined behaviour. -fno-omit-frame-pointer keeps the stack traces readable
# and -O1 is the optimisation level AddressSanitizer is tuned for.
SANFLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all \
           -fno-omit-frame-pointer -O1

.PHONY: all clean debug test test-debug format format-check lint install uninstall

all: $(TARGET)

$(TARGET): $(SOURCE) $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

# Built under a separate name so the sanitized and normal binaries can coexist
# without needing a clean in between.
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SOURCE) $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(SANFLAGS) $(SOURCE) -o $(DEBUG_TARGET)

clean:
	rm -f $(TARGET) $(DEBUG_TARGET)

test: $(TARGET)
	VERSION=$(VERSION) ./tests/run.sh

# The same suite, run against the sanitized binary. The test script takes the
# binary to exercise from BIN.
test-debug: $(DEBUG_TARGET)
	BIN=./$(DEBUG_TARGET) VERSION=$(VERSION) ./tests/run.sh

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
