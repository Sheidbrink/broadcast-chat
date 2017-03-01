#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "doublell.h"
#define QUEUE_SIZE 10
#define MAX_EVENTS 10
#define INIT_SOCK_BUF_SIZE 512
#define INC_BUF_SIZE 512

typedef struct {
	int fd;

	int buf_index;
	unsigned int buf_size;
	char *buf;

	int write_index;
	unsigned int write_size;
	char *write_buf;

	struct dll_node *nodeptr;
} my_socket;

void printHelp(char* argv[]) {
	printf("Usage: %s -p <port_num>\n", argv[0]);
}

void mylog(char* error) {
	printf("%s\n", error);
}

int setnonblocking(int client) {
	int flags, s;
	if((flags = fcntl(client, F_GETFL, 0)) == -1) {
		mylog("fcntl");
		return -1;
	}
	flags |= O_NONBLOCK;
	if(fcntl(client, F_SETFL, flags) == -1) {
		mylog("fcntl");
		return -1;
	}
	return 0;
}

void myclose(my_socket *client) {
	mylog("close client");
	close(client->fd);
	free(client->buf);
	free(client->write_buf);
	delete(client->nodeptr);
	free(client);
}

char* handle_client(my_socket *client) {
	ssize_t bytes_read = 0;
	unsigned char temp_buf[INIT_SOCK_BUF_SIZE];
	int remaining;
	char* needle;
	char* toReturn = NULL;
	int toReturnIndx = 0;
	while((bytes_read = read(client->fd, &temp_buf, INIT_SOCK_BUF_SIZE)) > 0) {
		if(bytes_read == 0) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			myclose(client);
			break;
		}
		else if(bytes_read == -1) {
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				break;
		}
		if(bytes_read + client->buf_index > client->buf_size) {
			client->buf_size *= 2;
			if((client->buf = realloc(client->buf, client->buf_size)) == NULL) {
				mylog("realloc");	
				exit(EXIT_FAILURE);
			}
		}
		memcpy(client->buf + client->buf_index, temp_buf, bytes_read);
		client->buf_index += bytes_read;
		client->buf[client->buf_index] = 0;
		needle = strstr(client->buf, "\n");
		if(needle != NULL) {
			*needle = 0;
			toReturn = malloc((needle+1 - client->buf)+1);
			strcpy(toReturn, client->buf);
			toReturnIndx = strlen(toReturn);
			toReturn[toReturnIndx] = '\n';
			toReturn[toReturnIndx+1] = 0;
			remaining = (needle+1-client->buf) - client->buf_index;
			memmove(client->buf, needle+1, remaining);
			client->buf_index = remaining;
		}
	}
	return toReturn;
}

int write_client(my_socket *client) {
	ssize_t bytes_sent = 0;
	while((bytes_sent = send(client->fd, client->write_buf, client->write_index, 0)) > 0) {
		memmove(client->write_buf, client->write_buf + bytes_sent, client->write_index - bytes_sent);
		client->write_index -= bytes_sent;
		client->write_buf[client->write_index] = 0;
	}
	if(client->write_index == 0)
		return 1;
	return 0;
}

void addToWriteBuffer(char* toWrite, my_socket* socket) {
	int lenToWrite = strlen(toWrite);
	while(lenToWrite + socket->write_index > socket->write_size) {
		socket->write_size *= 2;
		if((socket->write_buf = realloc(socket->write_buf, socket->write_size)) == NULL) {
			mylog("write realloc");
			exit(EXIT_FAILURE);
		}
	}
	memcpy(socket->write_buf + socket->write_index, toWrite, lenToWrite);
	socket->write_index += lenToWrite;
	socket->write_buf[socket->write_index] = 0;
}

int run_server(int sfd) {
	int lr = listen(sfd, QUEUE_SIZE);
	struct epoll_event ev, events[MAX_EVENTS];
	int c_sock; 		// to accept socket fd
	int epollfd, nfds;	// epoll fd and number of fds ready for event
	int n; 				// to loop through the events
	my_socket server;
	struct dll_node all_clients;
	all_clients.data = NULL;
	all_clients.next = NULL;
	all_clients.prev = NULL;
	struct dll_node *head;
	my_socket* new_client;
	char* toBroadcast;
	server.fd = sfd;
	server.buf = NULL;

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		mylog("epoll_create1");
		return EXIT_FAILURE;
	}
	ev.events = EPOLLIN;
	ev.data.ptr = &server;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server.fd, &ev) == -1) {
		mylog("epoll_ctl: listen_sock");
		return EXIT_FAILURE;
	}

	while ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) != -1) {
		for (n = 0; n < nfds; n++) {
			// Service the server
			if (((my_socket*)events[n].data.ptr)->fd == server.fd) {
				// NULL because we don't care whose connecting
				c_sock = accept(server.fd, NULL, NULL);
				if (c_sock == -1) {
					mylog("accept failure");
					exit(EXIT_FAILURE);
				}
				if(setnonblocking(c_sock) == -1) {
					mylog("nonblocking failure");
						exit(EXIT_FAILURE);
				}
				new_client = malloc(sizeof(my_socket));
				new_client->fd = c_sock;

				new_client->buf_size = INIT_SOCK_BUF_SIZE;
				new_client->buf = malloc(sizeof(char)*new_client->buf_size);
				new_client->buf_index = 0;
				memset(new_client->buf, 0, sizeof(new_client->buf));

				new_client->write_size = INIT_SOCK_BUF_SIZE;
				new_client->write_buf = malloc(sizeof(char)*new_client->write_size);
				new_client->write_index = 0;
				memset(new_client->write_buf, 0, sizeof(new_client->write_size));


				new_client->nodeptr = create(new_client);
				add(&all_clients, new_client->nodeptr);
				
				ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
				ev.data.ptr = new_client;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_client->fd, &ev) == -1) {
						mylog("epoll_ctl: c_sock");
						exit(EXIT_FAILURE);
				}
			}
			else {
				if(events[n].events & EPOLLIN) {
					toBroadcast = handle_client((my_socket *)events[n].data.ptr);
					if(toBroadcast != NULL) {
						// Send to all clients
						head = all_clients.next;
						printf("%s", toBroadcast);
						while(head != NULL) {
							if(head->data != events[n].data.ptr) {
								addToWriteBuffer(toBroadcast, (my_socket*) head->data);
								ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLOUT;
								ev.data.ptr = head->data;
								epoll_ctl(epollfd, EPOLL_CTL_MOD, ((my_socket*)head->data)->fd, &ev);
							}
							head = head->next;
						}
						free(toBroadcast);
					}
				}
				if(events[n].events & EPOLLOUT) {
					if(write_client((my_socket *)events[n].data.ptr)) {
						ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
						ev.data.ptr = events[n].data.ptr;
						epoll_ctl(epollfd, EPOLL_CTL_MOD, ((my_socket*)events[n].data.ptr)->fd, &ev);
					}
				}
				if(events[n].events & EPOLLRDHUP)
					myclose((my_socket*)events[n].data.ptr);
			}
		}
	}
}

int main(int argc, char* argv[]) {
	char opt;
	char* port = NULL;
	int sfd, toReturn;
	while ((opt=getopt(argc,argv,"p:"))!= -1) {
		switch (opt) {
			case 'p':
				port = optarg;
				break;
			default:
				printHelp(argv);
				exit(EXIT_FAILURE);
		}
	}
	if (port == NULL) {
		port = "8080";
	}
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		mylog("socket error");
		exit(EXIT_FAILURE);
	}
	
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	memset(&hints, 0, sizeof(struct addrinfo));
	// Unspecified ipv4 or ipv6
	hints.ai_family		= AF_UNSPEC;
	// TCP socket
	hints.ai_socktype	= SOCK_STREAM;
	// Any address?
	hints.ai_flags		= AI_PASSIVE;
	// any protocol (could be different stream implementations besides tcp)
	hints.ai_protocol	= 0;
	hints.ai_canonname	= NULL;
	hints.ai_addr		= NULL;
	hints.ai_next		= NULL;
	/*
 	 * If the AI_PASSIVE flag is specified in hints.ai_flags, and node is
     * NULL, then the returned socket addresses will be suitable for
     * bind(2)ing a socket that will accept(2) connections
     * 
     * NULL node is ADDR_ANY
 	 */ 
	printf("%s\n", port);
	s = getaddrinfo(NULL, port, &hints, &result);
	if (s != 0) {
		mylog("getaddrinfo error");
		//mylog(gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	
	for(rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		int value = 1;
		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int));
		if (sfd == -1)
			continue;
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(sfd);
	}
	
	if (rp == NULL || sfd == -1) {
		mylog("bind failed\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(result);
	toReturn = run_server(sfd);
	close(sfd);

	return toReturn;
}
