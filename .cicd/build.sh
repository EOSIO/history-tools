#!/bin/bash
set -e
export IMAGE="history-tools:$(git rev-parse HEAD)"
echo '+++ :docker: Building Container'
cd docker
echo "$ docker build -f ubuntu-18.04.dockerfile -t $IMAGE ."
docker build -f ubuntu-18.04.dockerfile -t "$IMAGE" .