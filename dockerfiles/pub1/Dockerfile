# Publisher 1
# -------------------------------------------------------- #
FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/publisher.c src/publisher.c
CMD set -x && \
    mkdir -p bin && \
    gcc -o ./bin/publisher src/publisher.c && \
    ./bin/publisher cpu-usage disk-usage -- pub1 automatic 6700 6701

