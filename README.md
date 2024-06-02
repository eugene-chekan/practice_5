# IPC Server-Client Program

This project demonstrates inter-process communication (IPC) using FIFOs (named pipes) between a server and multiple client processes. The server reads lines from a file (or standard input) and distributes them to client processes, which process the lines and send results back to the server.

## Table of Contents

- [Overview](#overview)
- [Compilation](#compilation)
- [Usage](#usage)
  - [Running the Server](#running-the-server)
  - [Running the Client](#running-the-client)
- [Example](#example)
- [Details](#details)
  - [Signals](#signals)
  - [FIFO Channels](#fifo-channels)
- [Cleanup](#cleanup)

## Overview

The program consists of two parts:
1. **Server**: Distributes lines of text to clients and collects the results.
2. **Client**: Receives lines of text, processes them by summing the digits, and sends the results back to the server.

## Compilation

To compile the server and client programs, use the following commands:

```sh
gcc -o server server.c
gcc -o client client.c
```

## Usage

### Running the Server

The server accepts either one or two arguments:

- <number_of_child_processes>: The number of client processes to create.
- [file_path]: (Optional) The path to the file to read lines from. If not provided, the server reads from stdin.

```sh
./server <number_of_child_processes>
```
or

```sh
./server <file_path> <number_of_child_processes>
```

### Running the Client

The client does not need to be run manually as it is started by the server. The server forks and execs the client processes automatically.

## Example

### Running the Server with a File

```sh
./server input.txt 3
```
This starts the server, which reads from input.txt and creates 3 client processes.

### Running the Server with Standard Input

```sh
./server 3
```

This starts the server, which reads from stdin and creates 3 client processes. Type lines of text and press Ctrl+D to end the input.

## Details

### Signals

The server uses signals to communicate with the client processes:

- *SIGUSR1*: Sent by the server to request intermediate results from the client.
- *SIGUSR2*: Sent by the server to request the final refults and signal the client to exit.

### FIFO Channels

The server and clients use FIFOs (named pipes) for communication:

- The server creates two FIFOs for each client: one for sending data to the client and one for receiving results from the client.
- The FIFOs are named `fifo_in_<n>` and `fifo_out_<n>` where `<n>` is the client number.

### Cleanup

To ensure proper cleanup of resources, the server registers a signal handler for *SIGINT* to handle interrupts (e.g., Ctrl+C). The handler stops all child processes, closes file descriptors, and removes FIFOs.