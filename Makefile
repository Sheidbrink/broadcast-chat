CC = gcc
CFLAGS = -g -Wall

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c -lscott

client: client.c
	$(CC) $(CFLAGS) -o client client.c -pthread -lscott

clean:
	rm -f server client
