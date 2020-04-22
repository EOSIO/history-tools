#!/bin/bash
set -e
echo '+++ :docker: Building Container'
cd docker
echo "$ docker build -f ubuntu-18.04.dockerfile -t \"history-tools:$(git rev-parse HEAD)\" ."
docker build -f ubuntu-18.04.dockerfile -t "history-tools:$(git rev-parse HEAD)" .