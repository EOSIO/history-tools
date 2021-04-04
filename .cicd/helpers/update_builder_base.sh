#!/bin/bash

docker_file=Dockerfile.builder_base
base_image_name=eosio/history-tools:builder_base

echo Pull docker image ${base_image_name} and rebuild it when necessary

set -x
set -e
DOCKER_CLI_EXPERIMENTAL=enabled

function get_docker_image_digest {
   docker manifest inspect $1 | jq -r '.config.digest' || :
}

function get_file_commit_id {
   # get the first 10 character of the latest commit id when the file $1 is changed
   git --no-pager log $1 | head -1 | cut -d' ' -f2 | cut -c-10
}

commit=$(get_file_commit_id $docker_file)
image=${base_image_name}_${commit}
digest=$(get_docker_image_digest ${image})

# if the docker image does not exist on the Docker Hub repo, build it and push it
if [ -z "$digest" ]; then
   docker build -t ${image} -f $docker_file .
   docker push ${image}
   digest=$(docker inspect --format='{{ .Id }}' ${image})
fi

# tag the image as '${base_image_name}' to be used on latter stages
docker tag ${image} ${base_image_name}

# push the docker image '${base_image_name}' only it's the master branch and 
# when the image has been changed
if [ $BUILDKITE_BRANCH == "master" ]; then
   master_builder_base_digest=$(get_docker_image_digest ${base_image_name})
   if [ $master_builder_base_digest != $digest ]; then
      docker push ${base_image_name}
   fi
fi
