#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir data 2> /dev/null

gcc src/build.cpp -o bin/dorfbook
cp -r data bin

