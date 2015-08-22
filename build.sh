#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir gen 2> /dev/null
mkdir data 2> /dev/null

cp -r data bin
gcc src/pre/pre_build.cpp -g -lm -lrt -lpthread -o bin/pre_dorfbook
bin/pre_dorfbook gen/
gcc src/build.cpp -D BUILD_DEBUG -g -lm -lrt -pthread -o bin/dorfbook

