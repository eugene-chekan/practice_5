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
#define LINE_SEPARATOR "\n******************************\n"

char *file_path;
FILE *input;
int num_processes;
int process = 0;
int times = 0;
int total_sum = 0;
pid_t *pids;
int *read_fds;
int *write_fds;
char **read_fifos;
char **write_fifos;

char fifo_in[BUFFER_SIZE], fifo_out[BUFFER_SIZE];
char out_buf[BUFFER_SIZE], in_buf[BUFFER_SIZE];

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
 * Creates a FIFO (named pipe).
 *
 * @param fifo_name The name of the FIFO to create.
 */
void create_fifo(char *fifo_name);

/**
 * Removes a FIFO (named pipe).
 *
 * @param fifo_name The name of the FIFO to remove.
 */
void remove_fifo(char *fifo_name);

/**
 * Handles the SIGINT signal to stop the server.
 *
 * @param sig The signal number.
 */
void stop_server(int sig);

int main(int argc, char *argv[]) {
    // Check the number of arguments and initialize file_path and num_processes
    if (argc == 2) {
        file_path = "stdin";
        num_processes = atoi(argv[1]);
        printf("No file provided. Will use %s\n", file_path);
    } else if (argc == 3) {
        file_path = argv[1];
        num_processes = atoi(argv[2]);
        printf("File path: %s\n", file_path);
    } else {
        fprintf(stderr, "Usage: %s [<file_path>] <number_of_child_processes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Number of child processes: %d\n", num_processes);
    printf(LINE_SEPARATOR);
    
    // Allocate memory for the array of child PIDs
    pids = malloc(num_processes * sizeof(pid_t));
    if (pids == NULL) {
        perror("Server failed to allocate memory for processes IDs\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for FIFO file descriptors and names
    read_fds = malloc(num_processes * sizeof(int));
    write_fds = malloc(num_processes * sizeof(int));
    read_fifos = (char **)malloc(num_processes * sizeof(char *));
    write_fifos = (char **)malloc(num_processes * sizeof(char *));

    // Register the SIGINT signal handler
    signal(SIGINT, stop_server);

    // Create FIFOs and fork child processes
    for (int i = 0; i < num_processes; i++) {
        snprintf(fifo_in, sizeof(fifo_in), "fifo_in_%d", i);
        snprintf(fifo_out, sizeof(fifo_out), "fifo_out_%d", i);

        create_fifo(fifo_in);
        create_fifo(fifo_out);

        pid_t pid = fork();
        if (pid == -1) {
            perror("Server failed to fork process\n");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) { // Child process
            execl("./client", "./client", fifo_in, fifo_out, (char *)NULL);
            perror("Server failed to start client process\n");
            exit(EXIT_FAILURE);
        } else { // Parent process
            pids[i] = pid;
            read_fifos[i] = strdup(fifo_in);
            write_fifos[i] = strdup(fifo_out);
            read_fds[i] = -1;
            write_fds[i] = -1;
        }
    }

    // Open input file or use stdin
    if (strcmp(file_path, "stdin") == 0) {
        input = stdin;
    } else {
        input = fopen(file_path, "r");
    }
    if (!input) {
        perror("Server failed to open file to read\n");
        stop_server(SIGINT);
        exit(EXIT_FAILURE);
    }

    // Main loop to read lines from the input and distribute them to child processes
    while (fgets(in_buf, sizeof(in_buf), input)) {
	    if (write_fds[process] < 0) {
		    write_fds[process] = open_fd(write_fifos[process], O_WRONLY);
	    }
        if (write(write_fds[process], in_buf, strlen(in_buf)) == -1) {
            perror("Server failed to write to fifo channel");
            stop_server(SIGINT);
            exit(EXIT_FAILURE);
        }
        if ((times++) % 10 == 0) {
            if (kill(pids[process], SIGUSR1) == -1) {
                perror("Server failed to send SIGUSR1");
                stop_server(SIGINT);
                exit(EXIT_FAILURE);
            }
            if (read_fds[process] < 0) {
                read_fds[process] = open_fd(read_fifos[process], O_RDONLY);
            }

            int bytes_read = read(read_fds[process], in_buf, sizeof(in_buf) - 1);
            if (bytes_read == -1) {
                perror("Server failed to read from buffer");
                stop_server(SIGINT);
                exit(EXIT_FAILURE);
            } else if (bytes_read > 0) {
                in_buf[bytes_read] = '\0';
                printf("Get result from process %s\n", in_buf);
            }
        }
        process = (process + 1) % num_processes;
    }

    // Close input file
    fclose(input);

    // Send SIGUSR2 to child processes to signal them to finish
    for (int i = 0; i < num_processes; i++) {
        if (kill(pids[i], SIGUSR2) < 0) {
            perror("Server failed to send SIGUSR2 signal\n");
            exit(EXIT_FAILURE);
        };

	    if (read_fds[i] < 0) {
            read_fds[i] = open_fd(read_fifos[i], O_RDONLY);
	    }
        int bytes_read;
        while ((bytes_read = read(read_fds[i], in_buf, sizeof(in_buf) - 1)) > 0) {
            in_buf[bytes_read] = '\0';
            printf("Final result from process %s\n", in_buf);
            
            // Variables to store the extracted integers
            int proc_num, result;
            
            // Parse the input string
            if (sscanf(in_buf, "%d: %d", &proc_num, &result) == 2) {
                
                // Add the second integer to the sum
                total_sum += result;
            } else {
                fprintf(stderr, "Server failed to parse result string: %s\n", in_buf);
            }
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_processes; i++) {
        waitpid(pids[i], NULL, 0);
        printf("Process %d finished.\n", pids[i]);
    }

    // Clean up FIFOs and file descriptors
    for (int i = 0; i < num_processes; i++) {
        close_fd(read_fds[i]);
        close_fd(write_fds[i]);
        remove_fifo(read_fifos[i]);
        remove_fifo(write_fifos[i]);
    }
    printf(LINE_SEPARATOR);
    printf("Final sum: %d\n", total_sum);

    // Free allocated memory
    free(pids);
    free(read_fds);
    free(write_fds);
    free(read_fifos);
    free(write_fifos);
    exit(EXIT_SUCCESS);
}

void create_fifo(char *fifo_name) {
    if (mkfifo(fifo_name, 0666) < 0 && errno != EEXIST) {
        fprintf(stderr, "Server failed to create %s channel\n", fifo_name);
        exit(EXIT_FAILURE);
    }
}

void remove_fifo(char *fifo_name) {
    if (unlink(fifo_name) == -1) {
        fprintf(stderr, "Server failed to remove %s channel\n", fifo_name);
        exit(EXIT_FAILURE);
    }
}

int open_fd(char *fchannel, int mode) {
    int fd;
    if ((fd = open(fchannel, mode)) == -1) {
        fprintf(stderr, "Server failed to open %s channel\n", fchannel);
        exit(EXIT_FAILURE);
    }
    return fd;
}

void close_fd(const int fd) {
    if (close(fd) == -1) {
        perror("Server failed to close fd");
        exit(EXIT_FAILURE);
    }
}

void stop_server(int sig) {
    for (int i = 0; i < num_processes; i++) {
        kill(pids[i], SIGUSR2);
    }
    for (int i = 0; i < num_processes; i++) {
        waitpid(pids[i], NULL, 0);
        printf("Process %d finished.\n", pids[i]);
    }

    for (int i = 0; i < num_processes; i++) {
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
    exit(EXIT_SUCCESS);
}
