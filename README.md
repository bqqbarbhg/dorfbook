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

Testing
-------

### Requirements

Python 2.x is required to run the tests. It should be included in most operating
systems except windows. To install on Windows you can download it at
https://www.python.org/downloads/

Also the following Python libraries are required:
- [requests](https://pypi.python.org/pypi/requests)
- [BeautifulSoup](https://pypi.python.org/pypi/beautifulsoup4)

The easiest way to install the depended libraries is to use pip. It is
included in newer Python versions, but if you have an older version follow the
instructions at
https://pip.pypa.io/en/latest/installing.html

With pip run the following to install the dependencies:
```
pip install -r test/python_requirements.txt
```

### Running

To run the test simply run the `test.py` file. It starts the server temporarily
and runs all the `.py` files under the `test/` folder.

