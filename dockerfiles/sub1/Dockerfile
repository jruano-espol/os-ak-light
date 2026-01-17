# Subscriber 1
# -------------------------------------------------------- #
FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/subscriber.c src/subscriber.c
COPY message.sh message.sh
CMD set -x && \
    mkdir -p bin && \
    gcc -o ./bin/subscriber src/subscriber.c && \
    ./bin/subscriber sub1 '#' 6400 7600 -persistent -threshold 1.2

