FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/publisher.c src/publisher.c
CMD set -xe && \
    gcc -o ./bin/publisher src/publisher.c && \
    publisher memory-usage login-error-logs -- pub2 localhost 6700

