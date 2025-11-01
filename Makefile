CC       := gcc
CFLAGS   := -O2 -Wall -Wextra -pedantic -std=c11 -pthread

TARGETS  := client1 client2
SRC1     := client1.c
SRC2     := client2.c

.PHONY: all clean

all: $(TARGETS)

client1: $(SRC1)
	$(CC) $(CFLAGS) -o $@ $^

client2: $(SRC2)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)