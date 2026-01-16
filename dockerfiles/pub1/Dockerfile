# Publisher 1
# -------------------------------------------------------- #
FROM gcc:15
COPY commands.txt commands.txt
COPY src/common.h src/common.h
COPY src/publisher.c src/publisher.c
CMD set -xe && \
    gcc -o ./bin/publisher src/publisher.c && \
    publisher cpu-usage disk-usage -- pub1 automatic 6700

