CC       := gcc
CFLAGS   := -O2 -Wall -Wextra -pedantic -std=c11 -pthread
TARGET   := client1
SRC      := client1.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
