#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 256

int connect_to_gateway(const char *host, const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;

    int status = getaddrinfo(host, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    int gateway_fd = -1;

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        gateway_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (gateway_fd < 0) continue;

        if (connect(gateway_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // success!
        }

        close(gateway_fd);
        gateway_fd = -1;
    }

    freeaddrinfo(res);

    if (gateway_fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%s\n", host, port);
    } else {
        printf("Connected to gateway at %s:%s\n", host, port);
    }

    return gateway_fd;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <gateway_host> <gateway_port>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    const char *port = argv[2];

    srand(time(NULL));

    while (1) {
        int fd = connect_to_gateway(host, port);
        if (fd < 0) {
            sleep(1);
            continue;
        }

        int temperature = rand() % 31 + 10; // 10 - 40 °C
        int humidity    = rand() % 81 + 20; // 20 - 100 %

        char msg1[BUFFER_SIZE];
        char msg2[BUFFER_SIZE];

        snprintf(msg1, sizeof(msg1), "pub1/temperature|%d°C\n", temperature);
        snprintf(msg2, sizeof(msg2), "pub1/humidity|%d%%\n", humidity);

        printf("Sending: %s", msg1);
        printf("Sending: %s", msg2);

        send(fd, msg1, strlen(msg1), 0);
        send(fd, msg2, strlen(msg2), 0);

        close(fd);

        sleep(1); // send every second
    }

    return 0;
}
