#!/bin/bash

set -e

function execute-psql-command(){
  POSTGRES_CONTAINER_ID=`docker-compose ps -q postgres`
  CMD="docker exec -it $POSTGRES_CONTAINER_ID psql $1 -c '$2'"
  RET_SQL_CMD=`eval $CMD`
}

if [[ "$BUILDKITE" == 'true' ]]; then
    apt-get update && apt-get -y install wget
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

bash $DIR/get_snapshot.sh
bash $DIR/create_env_file.sh

docker-compose up -d

for (( ; ; ))
do
  #check if there is data in database
  execute-psql-command "" "SELECT block_num FROM chain.block_info limit 5;"
  BLOCK_INFO_ROWS_COUNT=$RET_SQL_CMD

  if [[ $BLOCK_INFO_ROWS_COUNT =~ "5 rows" ]]; then
    #get block #s and check they are in ascending order

    execute-psql-command "-t" "SELECT array( SELECT block_num FROM chain.block_info limit 5 );"
    BLOCK_INFO_ROWS_CONTENT=`echo $RET_SQL_CMD | tr -d '{}'`

    LATEST_BLOCK=0
    while IFS=',' read -ra ADDR; do
      for CURRENT_BLOCK in "${ADDR[@]}"; do
        if [ $LATEST_BLOCK -gt $CURRENT_BLOCK ]; then
          exit 1
        fi
        LATEST_BLOCK=$CURRENT_BLOCK
      done
    done <<< "$BLOCK_INFO_ROWS_CONTENT"

    echo "Succesful test"
    break
  fi
  sleep 5
done