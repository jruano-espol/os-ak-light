FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/subscriber.c src/subscriber.c
CMD set -xe && \
    gcc -o ./bin/subscriber src/subscriber.c && \
    subscriber 127.0.0.1 6400 '#' 127.0.0.1 7600
