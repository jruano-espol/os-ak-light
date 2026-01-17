#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MB (1 << 20)
#define us_from_s(s) ((s) * 1000000)

#define MAX_MEMORY_SIZE (64 * MB)
#define MAX_SLEEP_TIME_SECONDS 20

int main() {
    srand(time(NULL));

    while (true) {
        const int size = rand() % (MAX_MEMORY_SIZE + 1);
        char* memory = malloc(size);
        for (size_t i = 0; i < size; i++) {
            memory[i] = 0xff;
        }
        free(memory);

        usleep(rand() % us_from_s(MAX_SLEEP_TIME_SECONDS));
    }
    return 0;
}
