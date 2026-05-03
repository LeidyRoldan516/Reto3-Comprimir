CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude
SRC = src/main.c src/huffman.c src/io.c src/editor.c
BIN = bin/editor

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f bin/editor src/*.o
