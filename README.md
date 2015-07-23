Dorfbook
========

[![Build Status](https://travis-ci.org/bqqbarbhg/dorfbook.svg?branch=master)](https://travis-ci.org/bqqbarbhg/dorfbook)

Virtual social media for following dwarven mischief.

Will be up at [http://dorfbook.com](http://dorfbook.com) later.

Building
--------

### Windows

Requires some version of Visual Studio compiler. You may need to adjust the
`call vcvarsall` line of `build.bat` to reflect your architecture and compiler
version.

Otherwise just running `build.bat` and starting `bin/dorfbook.exe` should
start the server.

### Linux

Currently only tested on Raspbian, but it should work fairly well with other
distros too, since it's not using anything too advanced.

Uses GCC for now, but could probably be switched to any C++ compiler which can
compile a single C++ file.

Running `build.sh` and starting `bin/dorfbook` should start the server.

### Other platforms

To add more platforms you need to create a `platform_$.cpp` file and include it
with a conditional check for that platform in `build.cpp`. Other code should be
platform independent so implementing the platform API and building `build.cpp`
with a C++ compiler of your choice should work.

