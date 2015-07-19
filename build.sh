#! /usr/bin/env bash

mkdir bin 2> /dev/null
mkdir data 2> /dev/null

g++ src/build.cpp -pthread -std=c++0x -o bin/dorfbook
cp -r data bin

