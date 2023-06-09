CC = gcc
CFLAGS = -Wall -Wextra -ledit
TARGET = bin/bb-lisp
SRC = main.c mpc.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	$(TARGET)

clean:
	rm -f $(TARGET)

