Dorfbook
========

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

### Other platforms

Currently `main.cpp` is the only Windows-specific file, it should be split
into a general HTTP serving and the platform parts.

After that just building build.cpp with the compiler of your choice should do
the trick.

