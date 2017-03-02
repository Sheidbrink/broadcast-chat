CC = gcc
CFLAGS = -g -Wall

all: server client

server: server.c doublell.c
	$(CC) $(CFLAGS) -o server server.c doublell.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c -pthread

clean:
	rm -f server client
