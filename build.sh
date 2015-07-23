#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir data 2> /dev/null

cp -r data bin
gcc src/build.cpp -g -lm -lrt -pthread -o bin/dorfbook

