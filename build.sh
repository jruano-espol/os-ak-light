#!/bin/sh

set -xe

mkdir -p bin
gcc -g -o ./bin/publisher ./src/publisher.c
gcc -g -o ./bin/broker ./src/broker.c
gcc -g -o ./bin/subscriber ./src/subscriber.c

# Testing
gcc -g -o ./bin/tests ./src/tests.c && ./bin/tests
