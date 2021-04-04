#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

[ -f snapshot.bin ] || ${DIR}/get_snapshot.sh
[ -f docker-compose.yaml ] || cp ${DIR}/../../docker-compose.yaml docker-compose.yaml
cd $DIR

export DOCKER_EOSIO_TAG=${1:-develop}
if [[ "$BUILDKITE" == 'true' ]]; then
  export DOCKER_HISTORY_TOOLS_TAG=$BUILDKITE_COMMIT
fi

docker-compose -f docker-compose.yaml -f docker-compose.test.yaml up --exit-code-from postgres-query