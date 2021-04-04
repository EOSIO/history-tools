#!/bin/bash

if git diff --name-only HEAD~1 HEAD | grep builder_base.dockerfile; then
   docker pull eosio/history-tools:builder_base || :
   docker build -t eosio/history-tools:builder_base -f ./builder_base.dockerfile .
   docker push eosio/history-tools:builder_base
fi