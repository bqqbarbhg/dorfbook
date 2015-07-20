#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir data 2> /dev/null

gcc src/build.cpp -lm -lrt -pthread -o bin/dorfbook
cp -r data bin

