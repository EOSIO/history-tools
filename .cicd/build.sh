#!/bin/bash
set -e
export IMAGE="history-tools:$(git rev-parse HEAD)"
echo '+++ :docker: Building Container'
cd docker
echo "$ docker build -f ubuntu-18.04.dockerfile -t $IMAGE ."
docker build -f ubuntu-18.04.dockerfile -t "$IMAGE" .
echo '+++ :package: Create rodeos Installer'
[[ -z "$PACKAGE_REVISION" ]] && export PACKAGE_REVISION='1'
[[ -z "$VERSION_STRING" ]] && export VERSION_STRING="$(docker run -it "$IMAGE" rodeos --version || : | sed 's/^v//' | sed "s/$/-$PACKAGE_REVISION/")"
echo "$ docker run -e BUILDKITE -w /eosio.cdt/libraries/history-tools -it \"$IMAGE\" ./scripts/package-rodeos.sh | tar -xC rodeos_$VERSION_STRING.deb"
docker run -e BUILDKITE -w /eosio.cdt/libraries/history-tools -it "$IMAGE" ./scripts/package-rodeos.sh | tar -xC rodeos_$VERSION_STRING.deb
echo '+++ :arrow_up: Uploading Artifacts'
buildkite-agent artifact upload *.deb