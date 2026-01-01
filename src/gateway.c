#include "common.h"
#include <signal.h>

int server_fd = -1;
int forward_fd = -1;
int client_fd = -1;

#define close_if_valid(fd) do { if ((fd) >= 0) { close(fd); } } while(0)

void handle_sigint(int sig) {
    close_if_valid(server_fd);
    close_if_valid(forward_fd);
    close_if_valid(client_fd);
    printfln("\nCleaned up socket file descriptors.");
    exit(0);
}

int main(int argc, const char** argv)
{
    signal(SIGINT, handle_sigint);

    if (argc - 1 != 3) {
        fprintf(stderr, "usage: %s <input_port> <output_hostname> <output_port>\n", argv[0]);
        exit(1);
    }

    int input_port = atoi(argv[1]);
    const char* output_hostname = argv[2];
    const char* output_port = argv[3];

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("ERROR: Creating server_fd failed");
        exit(1);
    }

    // Setup server address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(input_port),
    };

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR: Binding server_fd failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("ERROR: Listening to server_fd failed");
        exit(1);
    }

    printf("Listening on port %d...\n", input_port);

    while (true) {
        // Accept incoming connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("ERROR: Accepting server_fd failed");
            continue;
        }

        printf("\nPublisher connected to port %d.\n", input_port);

        forward_fd = connect_to_output(output_hostname, output_port);

        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            String text = (String){ .data = buffer, .length = bytes_read };
            ssize_t sent = forward_message(forward_fd, text, true);
            if (sent > 0) {
                printf("Forwarded to %s:%s\n", output_hostname, output_port);
            }
            printf("Publisher at port %d disconnected normally.\n", input_port);
        } else if (bytes_read < 0) {
            perror("ERROR: Reading publisher messages failed");
        }

        close(forward_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
