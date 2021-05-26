#!/bin/bash
set -euo pipefail

#Build commands for docker images
BUILD_BASE="docker build --target base --cache-from '${IMAGE_REPO}:builder_base-${DOCKER_FILE_COMMIT_SHORT_HASH}' --tag '${IMAGE_REPO}:builder_base-${DOCKER_FILE_COMMIT_SHORT_HASH}' --build-arg BUILDKIT_INLINE_CACHE=1 ."
echo "$ $BUILD_BASE"
eval $BUILD_BASE
BUILD_CACHE="docker build --cache-from '${IMAGE_REPO}:builder_base-${DOCKER_FILE_COMMIT_SHORT_HASH}' --cache-from '${IMAGE_REPO}:${IMAGE_BRANCH_TAG}' --tag '${IMAGE_REPO}:${GIT_SHORT_HASH}' --tag '${IMAGE_REPO}:${IMAGE_BRANCH_TAG}' --build-arg BUILDKIT_INLINE_CACHE=1 ."
echo "$ $BUILD_CACHE"
eval $BUILD_CACHE

#Push images
PUSH_BASE="docker push '${IMAGE_REPO}:builder_base-${DOCKER_FILE_COMMIT_SHORT_HASH}'"
echo "$ $PUSH_BASE"
eval $PUSH_BASE
PUSH_GIT="docker push '${IMAGE_REPO}:${GIT_SHORT_HASH}'"
echo "$ $PUSH_GIT"
eval $PUSH_GIT
PUSH_BRANCH="docker push '${IMAGE_REPO}:${IMAGE_BRANCH_TAG}'"
echo "$ $PUSH_BRANCH"
eval $PUSH_BRANCH

#Cleaning images after successfully pushed to ECR
echo " Clean up Docker Images... "
REM_BASE="docker rmi '${IMAGE_REPO}:builder_base-${DOCKER_FILE_COMMIT_SHORT_HASH}' || :"
REM_GIT="docker rmi '${IMAGE_REPO}:${GIT_SHORT_HASH}' || :"
REM_BRANCH="docker rmi '${IMAGE_REPO}:${IMAGE_BRANCH_TAG}' || :"
echo "$ Deleting.... "
echo "$ $REM_BASE"
eval $REM_BASE
echo "$ $REM_GIT"
eval $REM_GIT
echo "$ $REM_BRANCH"
eval $REM_BRANCH
