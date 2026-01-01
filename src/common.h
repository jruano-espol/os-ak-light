#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// Printing
// ------------------------------------------------------------------------------------------------------- //

#define printfln(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define fprintfln(f, fmt, ...) fprintf(f, fmt "\n", ##__VA_ARGS__)
#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define eprintfln(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

// Math
// ------------------------------------------------------------------------------------------------------- //

#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))

// Dynamic Array Macros
// ------------------------------------------------------------------------------------------------------- //

#define list_get(list, index)        ((list).data[(index)])
#define list_set(list, index, value) ((list).data[(index)] = (value))

#define list_reserve_add(list, size) do {\
    if ((list)->count + size > (list)->capacity) {\
        if ((list)->capacity == 0) {\
            (list)->capacity = 8;\
            (list)->data = malloc((list)->capacity * sizeof(*(list)->data));\
            assert((list)->data != NULL);\
        } else {\
            do { (list)->capacity *= 2; } while ((list)->count + size > (list)->capacity);\
            (list)->data = realloc((list)->data, (list)->capacity * sizeof(*(list)->data));\
            assert((list)->data != NULL);\
        }\
    }\
} while (0)

#define list_append(list, element) do {\
    list_reserve_add(list, 1);\
    (list)->data[(list)->count] = (element);\
    (list)->count++;\
} while (0)

#define list_destroy(list) do {\
    free((list)->data);\
    (list)->data = NULL;\
    (list)->count = (list)->capacity = 0;\
} while (0)

#define list_destroy_safely(list) do {\
    if ((list)->data != NULL) {\
        free((list)->data);\
        (list)->data = NULL;\
        (list)->count = (list)->capacity = 0;\
    }\
} while (0)

// Booleans
// ------------------------------------------------------------------------------------------------------- //

typedef struct { bool* data; size_t count, capacity; } bool_list;

// Strings
// ------------------------------------------------------------------------------------------------------- //

typedef struct {
    char* data;
    size_t length;
} String;

#define fmt_String(str) (int)(str).length, (str).data

typedef struct { String* data; size_t count, capacity; } String_list;

#define Strlen(str_literal) (sizeof(str_literal) - 1)
#define str8(str_literal) (String){ .data = (char*)(str_literal), .length = Strlen(str_literal) }
#define String_from_cstr(cstr) (String) { .data = (char*)(cstr), .length = strlen(cstr) }

#define String_get(str, index)        ((str).data[(index)])
#define String_set(str, index, value) ((str).data[(index)] = (value))

#define String_get_last(str)        String_get((str), (str).length - 1)
#define String_set_last(str, value) String_set((str), (str).length - 1, (value))

#define is_string_null(str) ((str).data == NULL)

#define string_destroy(str) do {\
    assert((str)->data != NULL);\
    free((str)->data);\
    (str)->data = NULL;\
    (str)->length = 0;\
} while (0)

String_list string_split(String const str, char delimiter)
{
    String_list parts = {0};

    size_t start = 0;
    for (size_t i = 0; i < str.length; i++) {
        if (String_get(str, i) == delimiter || i == str.length - 1) {
            String slice = { .data = str.data + start, .length = i - start };
            if (i == str.length - 1) {
                slice.length++;
            }
            list_append(&parts, slice);
            start = i + 1;
        }
    }

    return parts;
}

String string_clone(String const str)
{
    String result = {
        .data = malloc((str.length + 1) * sizeof(*str.data)),
        .length = str.length,
    };
    assert(result.data != NULL);
    memcpy(result.data, str.data, str.length);
    String_set(result, str.length, '\0');
    return result;
}

bool string_equals(String const a, String const b)
{
    if (a.length != b.length) {
        return false;
    }
    for (size_t i = 0; i < a.length; i++) {
        if (String_get(a, i) != String_get(b, i)) {
            return false;
        }
    }
    return true;
}

// Sockets
// ------------------------------------------------------------------------------------------------------- //

int connect_to_output(const char* output_hostname, const char* output_port)
{
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int status = getaddrinfo(output_hostname, output_port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "ERROR: getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    // Try each result until we connect
    int forward_fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        forward_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (forward_fd < 0) continue;

        if (connect(forward_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(forward_fd);
        forward_fd = -1;
    }

    freeaddrinfo(res);

    if (forward_fd < 0) {
        eprintfln("Failed to connect to forwarding target %s:%s\n", output_hostname, output_port);
    }
    return forward_fd;
}

ssize_t forward_message(int forward_fd, String message, bool show_recieved)
{
    if (show_recieved) {
        static int recieved_count = 1;
        printf("Recieved message: (%d)\"%.*s\"(%d)\n", recieved_count, fmt_String(message), recieved_count);
        recieved_count++;
    }

    // Send the received message to the output
    ssize_t sent = send(forward_fd, message.data, message.length, 0);
    if (sent < 0) {
        perror("ERROR: Forwarding message failed");
    }
    return sent;
}

// Topics
// ------------------------------------------------------------------------------------------------------- //

typedef struct {
    String original;
    String_list levels;
    int32_t multilevel_wildcard_index; 
} Topic;

#define PRI_Topic "%.*s"
#define fmt_Topic(topic) fmt_String((topic).original)

#define topic_get_level(topic, index) list_get((topic).levels, (index))

// Fast macro to check if a level has a single level wildcard.
#define topic_level_has_wildcard(topic, index)          \
    (topic_get_level((topic), (index)).length > 0 &&    \
     (*topic_get_level((topic), (index)).data == '+' || \
      *topic_get_level((topic), (index)).data == '#'))

bool is_topic_valid(Topic const topic)
{
    return !is_string_null(topic.original);
}

Topic parse_topic(String const text)
{
    if (string_equals(text, str8("#"))) {
        return (Topic){ 
            .original = str8("#"),
            .levels.count = 1,
            .multilevel_wildcard_index = 0,
        };
    }

    Topic topic = {
        .original = string_clone(text),
        .multilevel_wildcard_index = -1,
    };

    // It has to view the cloned string, not the one passed in.
    topic.levels = string_split(topic.original, '/');

    for (size_t i = 0; i < topic.levels.count; i++) {
        if (string_equals(list_get(topic.levels, i), str8("#"))) {
            // Multilevel wildcard errors.
            if (topic.multilevel_wildcard_index != -1) {
                eprintfln("ERROR: Topic has more than one multilevel wildcard: \"%.*s\"", fmt_String(text));
                goto had_error;
            } else if (i != topic.levels.count - 1) {
                eprintfln("ERROR: Topic has a multilevel wildcard that's not at the end: \"%.*s\"", fmt_String(text));
                goto had_error;
            }
            // Happy path.
            topic.multilevel_wildcard_index = i;
        }
    }

    return topic;

had_error:
    string_destroy(&topic.original);
    list_destroy(&topic.levels);
    return (Topic){};
}

bool topics_match(Topic const a, Topic const b) {
    size_t smaller_count = Min(a.levels.count, b.levels.count);
    bool a_has_multilevel = a.multilevel_wildcard_index >= 0;
    bool b_has_multilevel = b.multilevel_wildcard_index >= 0;

    size_t ignore_from;
    if (a_has_multilevel && b_has_multilevel) {
        ignore_from = Min(a.multilevel_wildcard_index, b.multilevel_wildcard_index);
        assert(ignore_from == smaller_count - 1);
    } else if (a_has_multilevel) {
        ignore_from = a.multilevel_wildcard_index;
        assert(ignore_from == smaller_count - 1);
    } else if (b_has_multilevel) {
        ignore_from = b.multilevel_wildcard_index;
        assert(ignore_from == smaller_count - 1);
    } else if (a.levels.count != b.levels.count) {
        return false;
    } else {
        ignore_from = smaller_count;
    }

    for (size_t i = 0; i < ignore_from; i++) {
        bool ignore = topic_level_has_wildcard(a, i) ||
                      topic_level_has_wildcard(b, i);
        if (!ignore && !string_equals(topic_get_level(a, i), topic_get_level(b, i))) {
            return false;
        }
    }
    return true;
}

// Publishers
// ------------------------------------------------------------------------------------------------------- //

typedef struct {
    Topic topic;
    String value;
} Publisher_Message;

#define PRI_Publisher_Message "(topic: " PRI_Topic ", value: \"%.*s\")"
#define fmt_Publisher_Message(msg) fmt_Topic((msg).topic), fmt_String((msg).value)

typedef struct {
    Publisher_Message* data;
    size_t count, capacity;
} Publisher_Message_list;

Publisher_Message_list get_messages_snapshot(Publisher_Message_list const messages, pthread_mutex_t* mutex)
{
    Publisher_Message_list snapshot = {};

    pthread_mutex_lock(mutex);
    snapshot.count = messages.count;
    if (snapshot.count > 0) {
        snapshot.capacity = snapshot.count;
        snapshot.data = (Publisher_Message*)malloc(snapshot.count * sizeof(*snapshot.data));
        assert(snapshot.data != NULL);
        for (size_t i = 0; i < snapshot.count; i++) {
            list_set(snapshot, i, list_get(messages, i));
        }
    }
    pthread_mutex_unlock(mutex);

    return snapshot;
}

bool is_publisher_message_valid(Publisher_Message const msg)
{
    return !is_string_null(msg.topic.original);
}

Publisher_Message parse_publisher_message(String const text)
{
    String_list parts = string_split(text, '|');
    if (parts.count != 2) {
        eprintfln("ERROR: Publisher message has %d parts instead of 2: \"%.*s\"", (int)parts.count, fmt_String(text));
        goto had_error;
    }

    Topic topic = parse_topic(list_get(parts, 0));
    if (!is_topic_valid(topic)) goto had_error;

    Publisher_Message message = {
        .topic = topic,
        .value = string_clone(list_get(parts, 1)),
    };
    list_destroy(&parts);
    return message;

had_error:
    list_destroy(&parts);
    return (Publisher_Message){};
}

// Subscribers
// ------------------------------------------------------------------------------------------------------- //

typedef struct {
    Topic topic;
    String output_hostname;
    String output_port;
} Subscriber_Message;

typedef struct {
    Subscriber_Message* data;
    size_t count, capacity;
} Subscriber_Message_list;

#define PRI_Subscriber_Message "(\"%.*s\", %.*s:%.*s)"
#define fmt_Subscriber_Message(msg) fmt_String((msg).topic.original), fmt_String((msg).output_hostname), fmt_String((msg).output_port)

Subscriber_Message* parse_subscriber_message(String const text)
{
    String_list parts = string_split(text, '|');
    if (parts.count != 2) {
        eprintfln("ERROR: Subscriber message has %d parts instead of 2: \"%.*s\"", (int)parts.count, fmt_String(text));
        goto had_error;
    }

    Topic topic = parse_topic(list_get(parts, 0));
    if (!is_topic_valid(topic)) goto had_error;
    
    String_list output_parts = string_split(list_get(parts, 1), ':');
    if (output_parts.count != 2) {
        eprintfln("ERROR: Subscriber message has %d output parts instead of 2: \"%.*s\"", (int)output_parts.count, fmt_String(text));
        goto had_error;
    }

    Subscriber_Message* message = (Subscriber_Message*)malloc(sizeof(*message));
    message->topic = topic;
    message->output_hostname = string_clone(list_get(output_parts, 0));
    message->output_port = string_clone(list_get(output_parts, 1));

    list_destroy(&output_parts);
    list_destroy(&parts);
    return message;

had_error:
    list_destroy(&output_parts);
    list_destroy(&parts);
    return NULL;
}

void subscriber_forward_message(Subscriber_Message const sub, Publisher_Message const message)
{
    if (topics_match(sub.topic, message.topic)) {
        char* buffer = malloc(BUFFER_SIZE);
        int chars_written = snprintf(buffer, BUFFER_SIZE, PRI_Publisher_Message, fmt_Publisher_Message(message));
        String to_send = { .data = buffer, .length = chars_written };

        int forward_fd = connect_to_output(sub.output_hostname.data, sub.output_port.data);
        if (forward_fd >= 0) {
            forward_message(forward_fd, to_send, false);
        }
        close(forward_fd);
        free(buffer);

        printfln("Subscriber " PRI_Subscriber_Message " accepted " PRI_Publisher_Message,
                fmt_Subscriber_Message(sub), fmt_Publisher_Message(message));

    } else {
        printfln("Subscriber " PRI_Subscriber_Message " rejected " PRI_Publisher_Message,
                fmt_Subscriber_Message(sub), fmt_Publisher_Message(message));
    }
}
