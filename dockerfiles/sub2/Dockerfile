# Subscriber 2
# -------------------------------------------------------- #
FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/subscriber.c src/subscriber.c
COPY message.sh message.sh
CMD set -xe && \
    gcc -o ./bin/subscriber src/subscriber.c && \
    subscriber sub2 '#' 6401 7601 -persistent -threshold 9.4

