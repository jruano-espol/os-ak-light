#include "common.h"

typedef struct {
    Publisher_Message_list messages;
    pthread_mutex_t messages_mutex;
    pthread_cond_t message_arrived;
} State;

static State ctx = {
    .messages_mutex = PTHREAD_MUTEX_INITIALIZER,
    .message_arrived = PTHREAD_COND_INITIALIZER,
};

void* publisher_connection(void* arg)
{
    assert(sizeof(void*) >= sizeof(int));
    int publisher_port = (int)(size_t)arg;

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("ERROR: socket failed");
        pthread_exit((void*)1);
    }

    // Setup server address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(publisher_port),
    };

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR: bind failed");
        pthread_exit((void*)1);
    }

    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("ERROR: listen failed");
        pthread_exit((void*)1);
    }

    printf("Listening publisher on port %d...\n", publisher_port);

    while (true) {
        // Accept incoming connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("ERROR: accept failed");
            continue;
        }

        printf("\nPublisher connected to port %d.\n", publisher_port);

        // Read data
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read, total_read = 0;
        while ((bytes_read = read(client_fd, buffer, BUFFER_SIZE)) > 0) {
            String text = (String){
                .data = buffer + total_read,
                .length = strlen(buffer + total_read),
            };
            String_list parts = string_split(text, '\n');
            for (size_t i = 0; i < parts.count; i++) {
                String part = list_get(parts, i);
                if (part.length == 0) continue; 
                if (String_get_last(part) == '\n') { part.length -= 1; }
                if (part.length == 0) continue; 

                Publisher_Message message = parse_publisher_message(part);
                if (is_publisher_message_valid(message)) {
                    pthread_mutex_lock(&ctx.messages_mutex);
                    list_append(&ctx.messages, message);
                    pthread_cond_broadcast(&ctx.message_arrived);
                    pthread_mutex_unlock(&ctx.messages_mutex);

                    printfln("Recieved message: " PRI_Publisher_Message, fmt_Publisher_Message(message));
                }
            }
            list_destroy(&parts);
            total_read += bytes_read;
        }

        if (bytes_read == 0) {
            printf("Publisher at port %d disconnected normally.\n", publisher_port);
        } else if (bytes_read < 0) {
            perror("ERROR: Reading publisher messages failed");
        }

        close(client_fd);
    }

    close(server_fd);
    return NULL;
}

void* subscriber_connection(void* arg)
{
    Subscriber_Message *sub_ptr = (Subscriber_Message*)arg;
    Subscriber_Message const sub = *sub_ptr;
    free(sub_ptr);

    printfln("Added Subscriber: " PRI_Subscriber_Message, fmt_Subscriber_Message(sub));

    // Forwarding messages already on the list
    if (sub.persistent) {
        Publisher_Message_list snapshot = get_messages_snapshot(ctx.messages, &ctx.messages_mutex);
        if (snapshot.count > 0) {
            for (size_t i = 0; i < snapshot.count; i++) {
                Publisher_Message message = list_get(snapshot, i);
                subscriber_forward_message(sub, message);
            }
            list_destroy(&snapshot);
        }
    }

    while (true) {
        pthread_mutex_lock(&ctx.messages_mutex);
        size_t last_count = ctx.messages.count;
        for (;;) {
            printfln("Subscriber " PRI_Subscriber_Message " waiting on new messages...", fmt_Subscriber_Message(sub));
            pthread_cond_wait(&ctx.message_arrived, &ctx.messages_mutex);
            if (last_count != ctx.messages.count) {
                printfln("Subscriber " PRI_Subscriber_Message " is about to inspect some messages...", fmt_Subscriber_Message(sub));
                break;
            }
        }
        size_t new_count = ctx.messages.count;

        if (sub.persistent) {
            size_t count = new_count - last_count;
            Publisher_Message* slice = (Publisher_Message*)malloc(count * sizeof(*slice));
            assert(slice != NULL);
            for (size_t i = 0; i < count; i++) {
                slice[i] = list_get(ctx.messages, last_count + i);
            }
            pthread_mutex_unlock(&ctx.messages_mutex);

            for (size_t i = 0; i < count; i++) {
                subscriber_forward_message(sub, slice[i]);
            }
            free(slice);
        } else {
            Publisher_Message message = list_get(ctx.messages, new_count - 1);
            pthread_mutex_unlock(&ctx.messages_mutex);
            subscriber_forward_message(sub, message);
        }
    }
}

void* listen_incoming_subscribers(void* arg)
{
    assert(sizeof(void*) >= sizeof(int));
    int listening_port = (int)(size_t)arg;

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        pthread_exit((void*)1);
    }

    // Setup server address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(listening_port),
    };

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        pthread_exit((void*)1);
    }

    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        pthread_exit((void*)1);
    }

    printf("Listening to subscribers on port %d...\n", listening_port);

    String_list existing_ports = {};

    while (true) {
        // Accept incoming connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        printf("\nSubscriber connected to listening port %d.\n", listening_port);

        // Read data
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            String text = (String){ .data = buffer, .length = bytes_read };
            if (text.data[text.length - 1] == '\n') {
                text.length -= 1;
                Subscriber_Message* subscriber = parse_subscriber_message(text);

                const bool create_thread = (subscriber != NULL) &&
                    (subscriber->persistent || !is_port_in(existing_ports, subscriber->output_port));

                if (create_thread) {
                    pthread_t subscriber_thread;
                    int result = pthread_create(&subscriber_thread, NULL, subscriber_connection, (void*)subscriber);
                    if (result == 0) {
                        list_append(&existing_ports, subscriber->output_port);
                    } else {
                        eprintfln("ERROR: Failed to create subscriber thread");
                        free(subscriber);
                    }
                }
            } else {
                eprintfln("ERROR: Message improperly terminated: \"%.*s\"", fmt_String(text));
            }
        }

        if (bytes_read == 0) {
            printf("Subscriber at listening port %d disconnected normally.\n", listening_port);
        } else if (bytes_read < 0) {
            perror("ERROR: Reading publisher messages failed");
        }

        close(client_fd);
    }

    close(server_fd);
    return NULL;
}

void usage(const char **argv)
{
    eprintfln("usage: %s message_storage_time subscriber_port [publisher_port ...]", argv[0]);
    eprintfln("\nmessage_sorage_time:");
    eprintfln(" - session: The messages never get removed from the list.");
    eprintfln(" - <x>s: The messages get removed from the list after <x> seconds.");
    exit(EXIT_FAILURE);
}

void *old_messages_cleaner(void *arg) {
    unsigned int wait_time = (unsigned int)(size_t)arg;

    while (true) {
        sleep(wait_time);
        time_t now = time(NULL);

        pthread_mutex_lock(&ctx.messages_mutex);
        if (ctx.messages.count > 0) {
            Publisher_Message first = list_get(ctx.messages, 0);
            time_t duration = now - first.timestamp;
            if (duration > wait_time) {
                printfln("Cleaned old message: " PRI_Publisher_Message, fmt_Publisher_Message(first));
                ctx.messages.count--;
                for (size_t i = 0; i < ctx.messages.count; i++) {
                    list_set(ctx.messages, i, list_get(ctx.messages, i + 1));
                }
            }
        }
        pthread_mutex_unlock(&ctx.messages_mutex);
    }
    return NULL;
}

int main(int argc, const char** argv)
{
    int const publisher_ports_offset = 3;
    int const publisher_ports_count = argc - publisher_ports_offset;

    if (argc - 1 < publisher_ports_offset) {
        usage(argv);
    }

    pthread_t cleaner_thread;
    bool has_cleaner_thread = false;
    /* Setting up how long messages can be stored */ {
        String message_storage_time = String_from_cstr(argv[1]);
        if (string_equals(message_storage_time, str8("session"))) {
            printfln("INFO: Messages last for the whole session");
        } else {
            int storage_time_seconds = storage_time_seconds = atoi(message_storage_time.data);
            if (storage_time_seconds <= 0) {
                eprintfln("ERROR: Messages can't last for %d seconds. Maybe had an error parsing an integer out of \"%s\"", storage_time_seconds, message_storage_time.data);
                usage(argv);
            }
            printfln("INFO: Messages last for %d seconds", storage_time_seconds);
            int result = pthread_create(&cleaner_thread, NULL, old_messages_cleaner, (void*)(size_t)storage_time_seconds);
            if (result != 0) {
                eprintfln("ERROR: Failed to create old message cleaner thread");
                exit(EXIT_FAILURE);
            }
            has_cleaner_thread = true;
        }
    }

    pthread_t listening_thread;
    /* Launch thread to listen to subscribers */ {
        int listening_port = atoi(argv[2]);
        int result = pthread_create(&listening_thread, NULL, listen_incoming_subscribers, (void*)(size_t)listening_port);
        if (result != 0) {
            eprintfln("ERROR: Failed to create subscriber listener thread");
            exit(EXIT_FAILURE);
        }
    }

    pthread_t* publisher_threads = malloc(publisher_ports_count * sizeof(pthread_t));

    for (int i = 0; i < publisher_ports_count; i++) {
        int publisher_port = atoi(argv[i + publisher_ports_offset]);
        int result = pthread_create(&publisher_threads[i], NULL, publisher_connection, (void*)(size_t)publisher_port);
        if (result != 0) {
            eprintfln("ERROR: Failed to create publisher listener thread[%d]", i);
            exit(EXIT_FAILURE);
        }
    }
    
    for (int i = 0; i < publisher_ports_count; i++) {
        pthread_join(publisher_threads[i], NULL);
    }
    pthread_join(listening_thread, NULL);
    pthread_join(listening_thread, NULL);

    if (has_cleaner_thread) {
        pthread_join(cleaner_thread, NULL);
    }

    return 0;
}
