#include "common.h"

typedef struct {
    String command;
    String command_name;
} Metric;

typedef struct {
    Metric *data;
    size_t count, capacity;
} Metric_list;

static String commands_txt;
static Metric_list metric_list;
static Metric_list used_metrics;

#define COMMANDS_FILENAME "commands.txt"

String run(Metric const metric);
void usage(char **argv);
void prelude(int argc, char** argv);
void print_metric_list(FILE *out, Metric_list const metrics);
int find_command_index(const char *name);
int connect_to_broker(const char *host, const char *port);
bool send_message(const char *host, const char *port, String_Builder const message);

int main(int argc, char **argv)
{
    prelude(argc, argv);

    if (argc <= 1) {
        usage(argv);
    }

    char **arg = &argv[1];
    for (; *arg; arg++) {
        if (strcmp(*arg, "--") == 0) {
            arg++;
            break;
        }
        int index = find_command_index(*arg);
        if (index < 0) {
            eprintfln("ERROR: The command \"%s\" is not in the command list.", *arg);
            eprintfln("\nThe available commands are:");
            print_metric_list(stderr, metric_list);
            exit(EXIT_FAILURE);
        }
        list_append(&used_metrics, list_get(metric_list, index));
    }

    if (used_metrics.count == 0) {
        eprintfln("ERROR: No commands were specified to run. They are needed to send messages.");
        exit(EXIT_FAILURE);
    }

    if (*arg == NULL) {
        eprintfln("ERROR: Did not provide a publisher name.\n");
        usage(argv);
    }
    const char *publisher_name = *arg;
    printfln("Using publisher name: %s", publisher_name);
    arg++;

    if (*arg == NULL) {
        eprintfln("ERROR: Did not provide a host.\n");
        usage(argv);
    }
    const char *host = *arg;
    printfln("Using host: %s", host);
    arg++;

    if (*arg == NULL) {
        eprintfln("ERROR: Did not provide a port.\n");
        usage(argv);
    }
    const char *port = *arg;
    printfln("Using port: %s", port);
    arg++;

    printfln("Sample runs:");
    for (size_t i = 0; i < used_metrics.count; i++) {
        Metric metric = list_get(used_metrics, i);
        String output = run(metric);
        printfln(" - " PRI_String ": " PRI_String, fmt_String(metric.command_name), fmt_String(output));
        string_destroy(&output);
    }
    printfln();

    while (true) {
        for (size_t i = 0; i < used_metrics.count; i++) {
            Metric metric = list_get(used_metrics, i);
            String output = run(metric);
            String_Builder message = {};
            string_builder_appendf(&message, "%s/" PRI_String "|" PRI_String "\n",
                    publisher_name, fmt_String(metric.command_name), fmt_String(output));
            list_append(&message, '\0');
            try_again:
            if (!send_message(host, port, message)) goto try_again;
            string_destroy(&output);
            string_builder_destroy(&message);
        }
    }
    
    return EXIT_SUCCESS;
}

void usage(char **argv)
{
    eprintfln("usage: %s [command ...] -- <publisher_name> <broker_host> <broker_port>", argv[0]);
    eprintfln("\nThe available commands are:");
    print_metric_list(stderr, metric_list);
    exit(EXIT_FAILURE);
}

void prelude(int argc, char** argv)
{
    if (!fs_read_entire_file(COMMANDS_FILENAME, &commands_txt)) {
        eprintfln("ERROR: The file \"%s\" must exist for this program to work.", COMMANDS_FILENAME);
        exit(EXIT_FAILURE);
    }

    String_list lines = string_split(commands_txt, '\n');
    if (!string_is_only_whitespace(list_get_last(lines))) {
        eprintfln("ERROR: The file \"%s\" must have an empty line at the end.", COMMANDS_FILENAME);
        exit(EXIT_FAILURE);
    }

    for (size_t line_index = 0; line_index < lines.count; line_index++) {
        String line = list_get(lines, line_index);
        if (!string_is_only_whitespace(line)) {
            assert(line.length > 0);
            Metric metric = { .command_name = line };
            if (String_get_last(metric.command_name) != ':') {
                eprintfln("%s:%zu: ERROR: Expected to terminate the command name with a colon `:` in " PRI_String_Quoted,
                        COMMANDS_FILENAME, line_index + 1, fmt_String(metric.command_name));
                exit(EXIT_FAILURE);
            }
            metric.command_name.length -= 1;
            if (metric.command_name.length == 0) {
                eprintfln("%s:%zu: ERROR: Empty command name",
                        COMMANDS_FILENAME, line_index + 1);
                exit(EXIT_FAILURE);
            }

            line_index += 1;
            line = list_get(lines, line_index);
            if (line_index >= lines.count) {
                eprintfln("%s:%zu: ERROR: Expected another line after the command name " PRI_String_Quoted,
                        COMMANDS_FILENAME, line_index, fmt_String(metric.command_name));
                exit(EXIT_FAILURE);
            }
            metric.command = line;
            list_append(&metric_list, metric);
        }
    }

    if (metric_list.count == 0) {
        eprintfln("ERROR: The file \"%s\" has no commands, it needs at least one.", COMMANDS_FILENAME);
        exit(EXIT_FAILURE);
    }

    list_destroy(&lines);
}

int find_command_index(const char *name)
{
    String const name_as_string = String_from_cstr(name);

    for (size_t i = 0; i < metric_list.count; i++) {
        Metric const metric = list_get(metric_list, i);
        if (string_equals(metric.command_name, name_as_string)) {
            return i;
        }
    }
    return -1;
}

void print_metric_list(FILE *out, Metric_list const metrics)
{
    for (size_t i = 0; i < metrics.count; i++) {
        Metric const metric = list_get(metrics, i);
        fprintfln(out, "    " PRI_String ": " PRI_String_Quoted,
                fmt_String(metric.command_name), fmt_String(metric.command));
    }
}

String run(Metric const metric)
{
    String null_terminated = string_clone(metric.command);

    FILE *file = popen(null_terminated.data, "r");
    if (file == NULL) {
        perror("ERROR: popen failed");
        exit(EXIT_FAILURE);
    }

    String_Builder builder = {};
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), file) != NULL ) {
        for (char *it = buffer; *it; it++) {
            list_append(&builder, *it);
        }
    }

    String output = String_from_builder(builder);
    if (output.length == 0) {
        return string_clone(str8("<nothing>"));
    }
    if (String_get_last(output) == '\n') {
        output.length -= 1;
    }

    pclose(file);
    string_destroy(&null_terminated);

    return output;
}

int connect_to_broker(const char *host, const char *port)
{
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

    int broker_fd = -1;

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        broker_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (broker_fd < 0) continue;

        if (connect(broker_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // success!
        }

        close(broker_fd);
        broker_fd = -1;
    }

    freeaddrinfo(res);

    if (broker_fd < 0) {
    } else {
        printf("Connected to broker at %s:%s\n", host, port);
    }

    return broker_fd;
}

bool send_message(const char *host, const char *port, String_Builder const message)
{
    int fd = connect_to_broker(host, port);
    if (fd < 0) {
        printfln("Could not connect to %s:%s, retrying...", host, port);
        sleep(1);
        return false;
    }

    printf("Sending: " PRI_String, fmt_String_Builder(message));
    send(fd, message.data, message.count, 0);

    close(fd);
    sleep(1); // send every second
    return true;
}
