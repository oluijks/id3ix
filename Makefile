# CC is deliberately not set here. Make defaults it to cc, the system compiler,
# and leaving it alone lets the environment or `make CC=...` choose one. CI
# pins the compilers it tests explicitly.
VERSION = 0.1.0
# _POSIX_C_SOURCE is needed because -std=c17 asks for strict ISO C, under which
# the C library hides POSIX interfaces. src/scan.c uses opendir, readdir and
# lstat, none of which are ISO C.
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -D_POSIX_C_SOURCE=200809L \
         -DID3IX_VERSION=\"$(VERSION)\"
PREFIX ?= /usr/local
TARGET = id3ix
DEBUG_TARGET = $(TARGET)-debug
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
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

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

# Every object depends on every header, which recompiles a little more than
# strictly necessary but never too little. Worth revisiting only if the build
# becomes slow enough to notice.
src/%.o: src/%.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

# Built under a separate name so the sanitized and normal binaries can coexist
# without needing a clean in between. Compiled straight from the sources rather
# than reusing src/*.o, because those objects are built without SANFLAGS and
# mixing the two would link successfully while instrumenting only part of the
# program.
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SOURCES) $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(SANFLAGS) $(SOURCES) -o $(DEBUG_TARGET)

clean:
	rm -f $(TARGET) $(DEBUG_TARGET) $(OBJECTS)

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
