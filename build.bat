@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x64
cd bin 2>NUL || echo Creating bin folder && mkdir bin && cd bin
mkdir data 2>NUL

set IgnoreWarn= -wd4100 -wd4101 -wd4189 -wd4706
set CLFlags= -MT -nologo -Od -W4 -WX -Zi %IgnoreWarn% -D_CRT_SECURE_NO_WARNINGS
set LDFlags= -opt:ref user32.lib gdi32.lib shell32.lib ws2_32.lib

cl %CLFlags% ../src/build.cpp -link %LDFlags% -out:dorfbook.exe
xcopy /qy ..\data data >NUL
cd ..

