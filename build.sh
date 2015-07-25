#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir pre 2> /dev/null
mkdir data 2> /dev/null

cp -r data bin
gcc src/pre/pre_build.cpp -g -lm -lrt -pthread -o bin/pre_dorfbook
bin/pre_dorfbook gen/pre_output.cpp
gcc src/build.cpp -g -lm -lrt -pthread -o bin/dorfbook

