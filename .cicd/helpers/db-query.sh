#!/bin/bash

set -e

function wait_nodeos_ready {
  for (( i=0 ; i<10; i++ )); do
    ! curl -fs http://nodeos:8888/v1/chain/get_info || break
    sleep 3
  done
}

function block_info_row_number {
  psql -c 'SELECT COUNT(*) from chain.block_info;' | head -3 | tail -1 | sed 's/^[ \t]*//'
}

wait_nodeos_ready

sleep 10

## first lets check the number of tables created in the chain schema
num_tables=$(psql -c "select count(*) from  pg_stat_all_tables where schemaname='chain'" | head -3 | tail -1 | sed 's/^[ \t]*//')

if [[ $num_tables < 24 ]]; then
  >&2 echo "expected at least 24 tables existed in the chain schema, only got ${num_tables}"
  exit 1
fi

sleep 30

psql -c "select relname from pg_stat_all_tables where schemaname='chain' and (n_tup_ins - n_tup_del)=0;" > empty_tables
let num_empty_tables="$( cat empty_tables | wc -l ) - 4"

if [[ $num_empty_tables > 3 ]]; then
  >&2 echo "expected no more than 3 empty tables, got $num_empty_tables empty tables"
  tail +3 empty_tables | head -n -2  >&2
  exit 1
fi

old_num_blocks=$(block_info_row_number)
sleep 10
if [[ $(block_info_row_number) = $old_num_blocks ]]; then
  >&2 echo the number of rows in block_info did not increase after 10 seconds
fi 
