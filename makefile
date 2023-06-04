CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = bin/bb-lisp
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	$(TARGET)

clean:
	rm -f $(TARGET)

