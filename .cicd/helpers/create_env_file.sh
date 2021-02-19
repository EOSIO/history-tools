#create .env file for docker-compose up command
echo "SNAPSHOT_FILE=$(pwd)/snapshots/$(ls snapshots)" > .env
echo "PEER_ADDR=seed.testnet.eos.io:9876" >> .env
echo "DOCKER_EOSIO_TAG=${1:-develop}" >> .env

if [[ "$BUILDKITE" == 'true' ]]; then
  echo "DOCKER_HISTORY_TOOLS_TAG=$BUILDKITE_COMMIT" >> .env
fi