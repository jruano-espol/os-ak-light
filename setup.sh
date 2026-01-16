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

IMAGES=(pub1 pub2 broker1 broker2 sub1 sub2)

clean_docker_images() {
    sudo docker system prune -f

    for img in "${IMAGES[@]}"; do
        sudo docker rmi -f "$img"
    done
}

build_docker_images() {
    for img in "${IMAGES[@]}"; do
        sudo docker build -t "$img" -f "./dockerfiles/$img/Dockerfile" .
    done
}

run_detached_docker_images() {
    for img in "${IMAGES[@]}"; do
        sudo docker run -d --network host --name "$img" "$img"
    done
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
