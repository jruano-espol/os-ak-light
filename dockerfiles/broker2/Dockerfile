# Broker 2
# -------------------------------------------------------- #
FROM gcc:15
COPY src/common.h src/common.h
COPY src/broker.c src/broker.c
CMD set -x && \
    mkdir -p bin && \
    gcc -o ./bin/broker src/broker.c && \
    ./bin/broker session 6401 6701

