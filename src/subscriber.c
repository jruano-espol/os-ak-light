#include "common.h"

int connect_to_broker(const char *host, int port) {
    int broker_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (broker_fd < 0) {
        perror("ERROR: broker socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        perror("ERROR: broker inet_pton");
        close(broker_fd);
        return -1;
    }

    if (connect(broker_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ERROR: broker connect");
        close(broker_fd);
        return -1;
    }

    return broker_fd;
}

int listen_to_broker(const char *host, int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("ERROR: listener socket");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        perror("ERROR: listener inet_pton");
        close(listen_fd);
        return -1;
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ERROR: listener bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("ERROR: listener listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int main(int argc, const char** argv)
{
    if (argc - 1 != 5) {
        eprintfln("usage: %s <broker_host> <broker_port> <topic> <listen_host> <listen_port>", argv[0]);
        exit(1);
    }

    const char *broker_host = argv[1];
    int broker_port = atoi(argv[2]);
    const char *topic = argv[3];
    const char *listen_host = argv[4];
    int listen_port = atoi(argv[5]);

    Topic parsed_topic = parse_topic(String_from_cstr(topic));
    if (!is_topic_valid(parsed_topic)) {
        exit(1);
    }

    printf("Subscriber starting...\n");
    printf(" - Broker: %s:%d\n", broker_host, broker_port);
    printf(" - Topic: %s\n", topic);
    printf(" - Listening on %s:%d\n", listen_host, listen_port);

    int listen_fd = listen_to_broker(listen_host, listen_port);
    if (listen_fd < 0) {
        eprintfln("Failed to start listener.");
        exit(1);
    }

    /* Sending the registration message to the Broker */ {
        int broker_fd = connect_to_broker(broker_host, broker_port);
        if (broker_fd < 0) {
            eprintfln("Could not connect to broker at %s:%d.", broker_host, broker_port);
            close(listen_fd);
            return 1;
        }

        char registration_message[256];
        snprintf(registration_message, sizeof(registration_message), "%s|%s:%d\n", topic, listen_host, listen_port);

        printf("Sending registration: %s\n", registration_message);
        send(broker_fd, registration_message, strlen(registration_message), 0);
        close(broker_fd);
    }

    while (true) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);

        int connection = accept(listen_fd, (struct sockaddr *)&client, &len);
        if (connection < 0) {
            perror("ERROR: listener accept");
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read = recv(connection, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printfln("Received message: %.*s", (int)bytes_read, buffer);
        }

        close(connection);
    }
    close(listen_fd);
}
