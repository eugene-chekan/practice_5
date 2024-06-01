#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 256

char *fifo_send;
char *fifo_recv;
int fd_recv = -1;
int fd_send = -1;
int cur_sum = 0;
bool user2_exit = false;

void handle_sigusr1(int sig) {
    char result[BUFFER_SIZE];
    printf("Sum of digits: %d\n", cur_sum);
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);

    if (fd_send < 0) {
	printf("open fifo_send\n");
        fd_send = open(fifo_send, O_WRONLY);
        if (fd_send == -1) {
            perror("open fifo_send");
            exit(EXIT_FAILURE);
	}
     }
    printf("[%d] sending back (%ld): %s\n",getpid(), strlen(result), result);
    write(fd_send, result, strlen(result));
}    

void handle_sigusr2(int sig) {
    printf("Termination signal received. Client PID: %d. exiting...\n", getpid());
    user2_exit = true;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <fifo_send> <fifo_recv>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fifo_send = argv[1];
    fifo_recv = argv[2];

    // Register signal handler
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    
    fd_recv = open(fifo_recv, O_RDONLY | O_NONBLOCK);
    if (fd_recv == -1) {
        perror("open fifo_recv");
        exit(EXIT_FAILURE);
    }
    struct timeval timeout;
    timeout.tv_sec = 5;//TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_recv, &read_fds);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while (!user2_exit) {

	if (select(fd_recv + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
            continue;
	}
	while ((bytes_read = read(fd_recv, buffer, sizeof(buffer) - 1)) > 0) {

		buffer[bytes_read] = '\0';
		printf("[%d] received: %d bytes -> %s\n", getpid(), bytes_read, buffer);
		// printf("Processing...\n");

		for (const char *p = buffer; *p; p++) {
			if (isdigit(*p)) {
				printf("Digit detected: %d\n", *p - '0');
				cur_sum += *p - '0';
			}
		}
	}
        // close(fd_recv);
    }

    printf("[%d] EXIT!!!!!!!!!!!\n", getpid());
    char result[BUFFER_SIZE];
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);
    if (fd_send < 0) {
        fd_send = open(fifo_send, O_WRONLY);
        if (fd_send == -1) {
            perror("open fifo_send");
            exit(EXIT_FAILURE);
	}
    }
    write(fd_send, result, strlen(result));
    close(fd_recv);
    close(fd_send);
    exit(EXIT_SUCCESS);
}
