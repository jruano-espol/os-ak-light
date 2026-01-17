# Publisher 2
# -------------------------------------------------------- #
FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/publisher.c src/publisher.c
COPY src/stress.c src/stress.c
CMD set -x && \
    mkdir -p bin && \
    gcc -o ./bin/publisher src/publisher.c && \
    gcc -o ./bin/stress src/stress.c ; \
    ./bin/stress & \
    ./bin/publisher memory-usage login-error-logs -- pub2 automatic 6701 6700

