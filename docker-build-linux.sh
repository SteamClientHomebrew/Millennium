#!/bin/bash

if docker build . -t millennium-builder:latest ; then
  printf "\n\tDocker Build Success"
else
  printf "\n\tDocker Build Failed"
  exit 1
fi


if docker run -d --name millennium-builder millennium-builder:latest ;then
  printf "\n\t Docker Container Started"
else
  printf "\n\tFailed to start Docker container"
  exit 1
fi 


docker cp millennium-builder:/home/runner/build-millennium.tar.gz .
docker stop millennium-builder
docker rm millennium-builder
docker image rm millennium-builder