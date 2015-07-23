
#include "prelude.h"

#ifdef _WIN32
#include "platform_windows.cpp"
#else
#include "platform_linux.cpp"
#endif

#include "deflate.cpp"
#include "random.cpp"
#include "dorf.cpp"
#include "main.cpp"

