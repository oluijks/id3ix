CC = clang
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g
TARGET = id3ix
SOURCE = src/main.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

format:
	clang-format -i src/*.c src/*.h

format-check:
	clang-format --dry-run --Werror src/*.c src/*.h
