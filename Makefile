CC = cc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g
TARGET = id3ix
SOURCE = src/main.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)