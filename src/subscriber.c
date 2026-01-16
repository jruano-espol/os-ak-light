#include "common.h"

typedef struct {
    const char *subscriber_name;
    const char *topic;
} State;

static State ctx = {};

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

void usage(const char **argv)
{
    eprintfln("usage: %s subscriber_name topic broker_port listen_port [flags ...]", argv[0]);
    eprintfln("\nflags:");
    eprintfln("    -persistent: Makes the session persistent. It is NOT persistent by default.");
    eprintfln("    -threshold <arg>: Enables sending whatsapp notifications when a message exceeds <arg> (which is an int).");
    eprintfln();
    exit(EXIT_FAILURE);
}

void send_threshold_twilio_message(const String message, double threshold)
{
    char command[1<<10];
    snprintf(command, sizeof command, "./message.sh 'In %s the message " PRI_String " exceeds the threshold of %g%%'",
            ctx.subscriber_name, fmt_String(message), threshold);
    system(command);
}

void notify_if_exceeds(const String message, double threshold)
{
    const String value_prefix = str8("value: \"");
    ssize_t value_index = string_find_substr(message, value_prefix);
    if (value_index < 0) {
        eprintfln("ERROR: Message does not have value.");
        return;
    }
    String value = {
        .data = message.data + value_index + value_prefix.length,
        .length = message.length - value_index - value_prefix.length,
    };
    const double message_threshold = strtod(value.data, NULL);
    if (message_threshold > threshold) {
        send_threshold_twilio_message(message, threshold);
    }
}

#define LOCALHOST "127.0.0.1"

int main(int argc, const char** argv)
{
    if (argc - 1 < 4) {
        usage(argv);
    }

    ctx.subscriber_name = argv[1];
    ctx.topic = argv[2];
    int broker_port = atoi(argv[3]);
    const char *broker_host = LOCALHOST;
    const char *listen_host = LOCALHOST;
    int listen_port = atoi(argv[4]);


    bool persistent = false;
    bool uses_threshold = false;
    double threshold = 0.0;

    for (const char **flag = &argv[5]; *flag != NULL; flag++) {
        if (strcmp(*flag, "-persistent") == 0) {
            persistent = true;
        } else if (strcmp(*flag, "-threshold") == 0) {
            uses_threshold = true;
            flag++;
            if (*flag == NULL) {
                eprintfln("ERROR: Must supply an argument to specify the threshold.\n");
                usage(argv);
            }
            threshold = strtod(*flag, NULL);
        } else {
            eprintfln("ERROR: Unrecognized flag \"%s\".\n", *flag);
            usage(argv);
        }
    }

    Topic parsed_topic = parse_topic(String_from_cstr(ctx.topic));
    if (!is_topic_valid(parsed_topic)) {
        exit(EXIT_FAILURE);
    }

    printf("Subscriber starting...\n");
    printf(" - Name: %s\n", ctx.subscriber_name);
    printf(" - Topic: %s\n", ctx.topic);
    printf(" - Broker: %s:%d\n", broker_host, broker_port);
    printf(" - Listening on %s:%d\n", listen_host, listen_port);
    printf(" - Persistent: %s\n", cstr_from_bool(persistent));
    if (uses_threshold) {
        printf(" - Threshold: %g\n", threshold);
    } else {
        printf(" - Threshold: Not existent\n");
    }

    int listen_fd = listen_to_broker(listen_host, listen_port);
    if (listen_fd < 0) {
        eprintfln("Failed to start listener.");
        exit(EXIT_FAILURE);
    }

    /* Sending the registration message to the Broker */ {
        int broker_fd = connect_to_broker(broker_host, broker_port);
        if (broker_fd < 0) {
            eprintfln("Could not connect to broker at %s:%d.", broker_host, broker_port);
            close(listen_fd);
            return 1;
        }

        char registration_message[256];
        snprintf(registration_message, sizeof(registration_message), "%s|%s:%d|%s\n",
                ctx.topic, listen_host, listen_port, persistent ? "p" : "-");

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
            const String message = { .data = buffer, .length = bytes_read };
            printfln("Received message: %.*s", fmt_String(message));

            if (uses_threshold) {
                notify_if_exceeds(message, threshold);
            }
        }

        close(connection);
    }

    close(listen_fd);
}
