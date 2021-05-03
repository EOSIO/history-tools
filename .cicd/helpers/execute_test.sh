#!/bin/bash

set -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd $DIR

[ -f snapshot.bin ] || ./get_snapshot.sh
[ -f docker-compose.yaml ] || cp ../../docker-compose.yaml .

export DOCKER_EOSIO_TAG=${1:-develop}
if [[ "$BUILDKITE" == 'true' ]]; then
  export DOCKER_HISTORY_TOOLS_TAG=$BUILDKITE_COMMIT
fi

docker-compose -f docker-compose.yaml -f docker-compose.test.yaml up -V --exit-code-from postgres-query
