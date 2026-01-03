#!/bin/bash

clean_docker_containers() {
    container_ids=$(sudo docker ps -aq)

    if [ -n "$container_ids" ]; then
        sudo docker kill $container_ids
        sudo docker rm $container_ids
    else
        echo "No containers to clean."
    fi
}

clean_docker_images() {
    sudo docker system prune -f

    sudo docker rmi -f pub1
    sudo docker rmi -f pub2
    sudo docker rmi -f broker1
    sudo docker rmi -f sub1
}

build_docker_images() {
    sudo docker build -t pub1    -f ./publishers/pub1/Dockerfile    .
    sudo docker build -t pub2    -f ./publishers/pub2/Dockerfile    .
    sudo docker build -t broker1 -f ./publishers/broker1/Dockerfile .
    sudo docker build -t sub1    -f ./publishers/sub1/Dockerfile    .
}

run_detached_docker_images() {
    sudo docker run -d --network host --name pub1    pub1
    sudo docker run -d --network host --name pub2    pub2
    sudo docker run -d --network host --name broker1 broker1
    sudo docker run -d --network host --name sub1    sub1
}

if [ "$1" == "clean-images" ]; then
    clean_docker_containers
    clean_docker_images
    build_docker_images
    run_detached_docker_images
elif [ "$1" == "clean" ]; then
    clean_docker_containers
else
    clean_docker_containers
    set -xe
    build_docker_images
    run_detached_docker_images
fi
