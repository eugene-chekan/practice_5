#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define BUFFER_SIZE 256

int num_processes;
pid_t *pids;
int *read_fds;
int *write_fds;
// char *read_fifos[num_processes];
// char *write_fifos[num_processes];

char fifo_in[BUFFER_SIZE], fifo_out[BUFFER_SIZE];
char out_buf[BUFFER_SIZE];
char in_buf[BUFFER_SIZE];

int open_fd(char *fchannel, int mode);
void close_fd(int fd);
void create_fifo(char *fifo_name);
void remove_fifo(char *fifo_name);
void stop_server(int sig);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <number_of_child_processes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *file_path = argv[1];
    num_processes = atoi(argv[2]);

    printf("File path: %s\n", file_path);
    printf("Number of child processes: %d\n", num_processes);
    
    // Allocate memory for the array of child PIDs
    pids = malloc(num_processes * sizeof(pid_t));
    printf("Allocated memory\n");
    if (pids == NULL) {
        perror("malloc");
        exit(1);
    }

    read_fds = malloc(num_processes * sizeof(int));
    write_fds = malloc(num_processes * sizeof(int));
    char **read_fifos = (char **)malloc(num_processes * sizeof(char *));
    char **write_fifos = (char **)malloc(num_processes * sizeof(char *));
    // read_fifos = malloc(num_processes * sizeof(const char) * BUFFER_SIZE);
    // write_fifos = malloc(num_processes * sizeof(const char) * BUFFER_SIZE);

    signal(SIGINT, stop_server);

    for (int i = 0; i < num_processes; i++) {
        snprintf(fifo_in, sizeof(fifo_in), "fifo_in_%d", i);
        snprintf(fifo_out, sizeof(fifo_out), "fifo_out_%d", i);

        create_fifo(fifo_in);
        create_fifo(fifo_out);

        //int read_fd = open_fd(fifo_in, O_RDONLY | O_NONBLOCK);
        //int write_fd = open_fd(fifo_out, O_WRONLY);

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) { // Child process
            execl("./client", "./client", fifo_in, fifo_out, (char *)NULL);
            perror("execl");
            exit(EXIT_FAILURE);
        } else {
            pids[i] = pid;
            read_fifos[i] = strdup(fifo_in);
            write_fifos[i] = strdup(fifo_out);
            read_fds[i] = -1;//read_fd;
            write_fds[i] = -1;
            printf("Process %d started with PID %d\n", i, pid);
        }
    }

    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int process = 0;
    while (fgets(in_buf, sizeof(in_buf), file)) {
        printf("Sending to process %d (%d): %s\n", process, (int)strlen(in_buf), in_buf);
	if (write_fds[process] < 0) {
		write_fds[process] = open_fd(write_fifos[process], O_WRONLY);
	}
        if (write(write_fds[process], in_buf, strlen(in_buf)) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        printf("Send signal SIGUSR1 to process %d\n", pids[process]);
        if (kill(pids[process], SIGUSR1) == -1) {
            perror("kill");
            exit(EXIT_FAILURE);
        }

	if (read_fds[process] < 0) {
            read_fds[process] = open(read_fifos[process], O_RDONLY);
            if (read_fds[process] == -1) {
                perror("open fifo_in");
                exit(EXIT_FAILURE);
            }
	}
        int bytes_read = read(read_fds[process], in_buf, sizeof(in_buf) - 1);
        if (bytes_read == -1) {
            perror("read1");
            exit(EXIT_FAILURE);
        } else if (bytes_read > 0) {
            in_buf[bytes_read] = '\0';
            printf("Result from process %d (%d): %s\n", pids[process], bytes_read, in_buf);
	}

        process = (process + 1) % num_processes;
    }

    fclose(file);
    sleep(1); // give some time for clients

    for (int i = 0; i < num_processes; i++) {
        printf("Sending closing signal to [%d] child process...\n", pids[i]);
        kill(pids[i], SIGUSR2);

        // snprintf(fifo_in, sizeof(fifo_in), "fifo_in_%d", i);
	if (read_fds[i] < 0) {
            read_fds[i] = open(read_fifos[i], O_RDONLY);
            if (read_fds[i] == -1) {
                perror("open fifo_in");
                exit(EXIT_FAILURE);
            }
	}
        int bytes_read;
        while ((bytes_read = read(read_fds[i], in_buf, sizeof(in_buf) - 1)) > 0) {
            // while (!bytes_read) {
            //     // sleep(1);
            //     printf("No info from process %d", pids[i]);
            //     continue;
            // }
            in_buf[bytes_read] = '\0';

            printf("Final result from process %d (%d): %s\n", pids[i], bytes_read, in_buf);
        }
        // close(fd_in);
    }

    // sleep(2);
    // Wait for all child processes to finish
    printf("Waiting for child processes to finish...\n");
    for (int i = 0; i < num_processes; i++) {
        waitpid(pids[i], NULL, 0);
        printf("Process %d finished.\n", pids[i]);
    }

    for (int i = 0; i < num_processes; i++) {
        // char fifo_in[BUFFER_SIZE], fifo_out[BUFFER_SIZE];
        // snprintf(fifo_in, sizeof(fifo_in), "fifo_in_%d", i);
        // snprintf(fifo_out, sizeof(fifo_out), "fifo_out_%d", i);
        close_fd(read_fds[i]);
        close_fd(write_fds[i]);
        remove_fifo(read_fifos[i]);
        remove_fifo(write_fifos[i]);
    }

    free(pids);
    free(read_fds);
    free(write_fds);
    free(read_fifos);
    free(write_fifos);
    exit(0);
}

void create_fifo(char *fifo_name) {
    if (mkfifo(fifo_name, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    printf("%s FIFO file created\n", fifo_name);
}

void remove_fifo(char *fifo_name) {
    if (unlink(fifo_name) == -1) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }
}

int open_fd(char *fchannel, int mode) {
    int fd;
    if ((fd = open(fchannel, mode)) == -1) {
        perror("open fd");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void close_fd(const int fd) {
    if (close(fd) == -1) {
        perror("close fd");
        exit(EXIT_FAILURE);
    }
}

void stop_server(int sig) {
    printf("Received signal %d\n", sig);
    for (int i = 0; i < num_processes; i++) {
        printf("Sending signal to [%d] child process...\n", pids[i]);
        kill(pids[i], SIGUSR2);
    }
}
