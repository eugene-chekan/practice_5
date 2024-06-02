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
char buffer[BUFFER_SIZE];
int bytes_read;

/**
 * Opens a file descriptor for the given FIFO channel.
 *
 * @param fchannel The name of the FIFO channel.
 * @param mode The mode in which to open the FIFO.
 * @return The file descriptor for the opened FIFO.
 */
int open_fd(char *fchannel, int mode);

/**
 * Closes the given file descriptor.
 *
 * @param fd The file descriptor to close.
 */
void close_fd(int fd);

/**
 * Signal handler for SIGUSR1.
 *
 * @param signum The signal number.
 */
void handle_sigusr1(int signum);

/**
 * Signal handler for SIGUSR2.
 *
 * @param signum The signal number.
 */
void handle_sigusr2(int signum);

/**
 * Cleanly exits the program by closing file descriptors and terminating.
 *
 * @param exit_status The status code to exit with.
 */
void clean_exit(int exit_status);


int main(int argc, char *argv[]) {
    // Check for correct number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <fifo_send> <fifo_recv>\n", argv[0]);
        clean_exit(EXIT_FAILURE);
    }

    fifo_send = argv[1];
    fifo_recv = argv[2];

    // Register signal handlers
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    
    // Open the receive FIFO in non-blocking read mode
    fd_recv = open_fd(fifo_recv, O_RDONLY | O_NONBLOCK);

    // Initialize a timeout structure for the select call.
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // Initialize a file descriptor set for select.
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_recv, &read_fds);

    // Main loop to read data from the receive FIFO
    while (!user2_exit) {
	timeout.tv_sec = 1;
        // Use select to wait for data on the receive FIFO, read the data, and sum the digits.
    	if (select(fd_recv + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
	        continue;
    	}
        if (!FD_ISSET(fd_recv, &read_fds)) {
            continue;
        }
        while ((bytes_read = read(fd_recv, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            
            // Process each character in the buffer, parse digits and add to sum
            for (const char *p = buffer; *p; p++) {
                if (isdigit(*p)) {
                    cur_sum += *p - '0';
                }
            }
        }
    }

    // Prepare and send the result when user2_exit is true
    char result[BUFFER_SIZE];
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);
    if (fd_send < 0) {
        fd_send = open_fd(fifo_send, O_WRONLY);
    }
    write(fd_send, result, strlen(result));

    // Clean up and finish the program
    clean_exit(EXIT_SUCCESS);
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
        clean_exit(EXIT_FAILURE);
    }
    return fd;
}

void close_fd(const int fd) {
    if (close(fd) == -1) {
        perror("Client failed to close fd");
        clean_exit(EXIT_FAILURE);
    }
}

void clean_exit(int exit_status) {
    close_fd(fd_recv);
    close_fd(fd_send);
    exit(exit_status);
}
