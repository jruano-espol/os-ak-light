FROM gcc:15
COPY src/common.h src/common.h
COPY src/broker.c src/broker.c
CMD set -xe && \
    gcc -o ./bin/broker src/broker.c && \
    broker session 6401 6701
