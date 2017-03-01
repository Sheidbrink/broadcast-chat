CC = gcc
CFLAGS = -g

all: server

server: server.c doublell.c
	$(CC) $(CFLAGS) -o server server.c doublell.c

clean:
	rm -f server
