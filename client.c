#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#define BUFFER_SIZE 256

char *fifo_send;
char *fifo_recv;
int fd_recv;
int fd_send;
int cur_sum = 0;

void handle_sigusr1(int sig) {
    char result[BUFFER_SIZE];
    // printf("Sum of digits: %d\n", cur_sum);
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);

    // printf("Opening channel %s for sending to client\n", fifo_send);
    // int fd_send = open(fifo_send, O_WRONLY);
    // if (fd_send == -1) {
    //     perror("open fifo_send");
    //     exit(EXIT_FAILURE);
    // }
    // printf("[%d] sending back (%d): %s\n",getpid(), strlen(result), result);
    write(fd_send, result, strlen(result));
}    

void handle_sigusr2(int sig) {
    char result[BUFFER_SIZE];
    // printf("[%d] Total sum: %d\n", getpid(), cur_sum);
    // printf("Termination signal received. Client PID: %d. exiting...\n", getpid());
    snprintf(result, sizeof(result), "%d: %d", getpid(), cur_sum);

    // fd_send = open(fifo_send, O_WRONLY | O_NONBLOCK);
    // if (fd_send == -1) {
    //     perror("open fifo_send");
    //     exit(EXIT_FAILURE);
    // }
    // printf("[%d] sending back total (%d): %s\n",getpid(), strlen(result), result);
    write(fd_send, result, strlen(result));
    close(fd_send);
    close(fd_recv);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <fifo_send> <fifo_recv>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fifo_send = argv[1];
    fifo_recv = argv[2];
    // printf("Client [%d] started with args: %s, %s\n", getpid(), fifo_send, fifo_recv);

    fd_recv = open(fifo_recv, O_RDONLY);
    if (fd_recv == -1) {
        perror("open fifo_recv");
        exit(EXIT_FAILURE);
    }
    
    fd_send = open(fifo_send, O_WRONLY);
    if (fd_send == -1) {
        perror("open fifo_send");
        close(fd_recv);
        exit(EXIT_FAILURE);
    }
    // Register signal handler
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    
    while (1) {
        // int fd_recv = open(fifo_recv, O_RDONLY);
        // if (fd_recv == -1) {
        //     perror("open fifo_recv");
        //     exit(EXIT_FAILURE);
        // }

        char buffer[BUFFER_SIZE];
        int bytes_read = read(fd_recv, buffer, sizeof(buffer) - 1);
        if (!bytes_read) {
            continue;
        }

        buffer[bytes_read] = '\0';
        printf("[%d] received: %d bytes -> %s\n", getpid(), bytes_read, buffer);
        // printf("Processing...\n");
        
        for (const char *p = buffer; *p; p++) {
            if (isdigit(*p)) {
                printf("Digit detected: %d\n", *p - '0');
                cur_sum += *p - '0';
            }
        }
        // close(fd_recv);
    }

    close(fd_recv);
    close(fd_send);
    exit(0);
}
