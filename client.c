#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <scott/utils.h>

#define DELETE_CHAR 0x7F
#define ESCAPE_CHAR 27

pthread_mutex_t console_mut;
int running;
char* input_buf;
int input_index = 0;
size_t input_size;
int sock;

void* thread_recv(void *p) {
	char* chat_buf = NULL;
	void* chat_bufv;
	size_t chat_buf_size = 0;
	ssize_t total_read;
	char recv_buf[512];
	ssize_t bytes_read;

	while(running) {
		total_read = 0;
		do {
			bytes_read = recv(sock, recv_buf, 512, 0);
			if(bytes_read <= 0) {
				running = 0;
				break;
			}
			chat_bufv = &chat_buf;
			chat_buf_size = copy_to_buffer(chat_bufv, chat_buf_size, total_read, recv_buf, bytes_read+1);
			total_read += bytes_read;
			chat_buf[total_read] = 0;
		} while(strstr(chat_buf, "\n") == NULL);
		pthread_mutex_lock(&console_mut);
		printf("%c[2K\r%s", ESCAPE_CHAR, chat_buf);
		if(input_index > 0)
			printf("%s", input_buf);
		fflush(stdout);
		pthread_mutex_unlock(&console_mut);
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	char c;
	static struct termios oldt, newt;
	
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int ret;
	char *host, *port;
	host = argv[1];
	port = argv[2];

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	ret = getaddrinfo(host, port, &hints, &result);
	if (ret != 0) {
		fprintf(stderr, "Failed to getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	for(rp = result; rp != NULL; rp = result->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock == -1) {
			perror("socket");
			continue;
		}
		if(connect(sock, rp->ai_addr, rp->ai_addrlen) == -1) {
			perror("connect");
			close(sock);
			continue;
		}
		break;
	}

	if(rp == NULL || sock == -1) {
		fprintf(stderr, "Failed to create socket\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	input_buf = malloc(512);
	input_size = 512;
	running = 1;

	pthread_t recver;
	pthread_mutex_init(&console_mut, NULL);
	pthread_create(&recver, NULL, &thread_recv, NULL);

	int total_sent;
	int bytes_sent;

	while(running) {
		c = getchar();
		pthread_mutex_lock(&console_mut);
		switch(c) {
			case 127:
				if(input_index > 0) {
					printf("\b \b");
					fflush(stdout);
					input_buf[--input_index] = 0;
				}
				break;
			case '\n':
				input_buf[input_index++] = c;
				printf("%c", c);
				total_sent = 0;
				input_buf[input_index] = 0;
				while(total_sent < input_index) {
					bytes_sent = send(sock, input_buf, input_index - total_sent,0);
					total_sent += bytes_sent;
					if(bytes_sent <= 0) {
						perror("send");
						break;
					}
				}
				input_index = 0;
				break;
			default:
				input_buf[input_index++] = c;
				printf("%c", c);
				fflush(stdout);
				break;
		}
		if(input_index >= input_size) {
			input_size *= 2;
			input_buf = realloc(input_buf, input_size);
		}
		pthread_mutex_unlock(&console_mut);
	}
	running = 0;
	pthread_join(recver, NULL);
	pthread_mutex_destroy(&console_mut);
	free(input_buf);

	close(sock);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return 0;
}
