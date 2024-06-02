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

int open_fd(char *fchannel, int mode);
void close_fd(int fd);
void handle_sigusr1(int signum);
void handle_sigusr2(int signum);


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <fifo_send> <fifo_recv>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fifo_send = argv[1];
    fifo_recv = argv[2];

    // Register signal handlers
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    
    fd_recv = open_fd(fifo_recv, O_RDONLY | O_NONBLOCK);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_recv, &read_fds);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while (!user2_exit) {
	timeout.tv_sec = 1;
    	if (select(fd_recv + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
	        continue;
    	}
        if (!FD_ISSET(fd_recv, &read_fds)) {
            continue;
        }
        while ((bytes_read = read(fd_recv, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';

            for (const char *p = buffer; *p; p++) {
                if (isdigit(*p)) {
                    cur_sum += *p - '0';
                }
            }
        }
    }

    char result[BUFFER_SIZE];
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);
    if (fd_send < 0) {
        fd_send = open_fd(fifo_send, O_WRONLY);
    }
    write(fd_send, result, strlen(result));
    close_fd(fd_recv);
    close_fd(fd_send);
    exit(EXIT_SUCCESS);
}

void handle_sigusr1(int signum) {
    char result[BUFFER_SIZE];
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);

    if (fd_send < 0) {
        fd_send = open_fd(fifo_send, O_WRONLY);
    }
    write(fd_send, result, strlen(result));
}

void handle_sigusr2(int signum) {
    user2_exit = true;
}

int open_fd(char *fchannel, int mode) {
    int fd;
    if ((fd = open(fchannel, mode)) == -1) {
        fprintf(stderr, "Client failed to open %s channel\n", fchannel);
        exit(EXIT_FAILURE);
    }
    return fd;
}

void close_fd(const int fd) {
    if (close(fd) == -1) {
        perror("Client failed to close fd");
        exit(EXIT_FAILURE);
    }
}
