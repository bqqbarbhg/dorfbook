
#include "prelude.h"

#ifdef _WIN32
#include "platform_windows.cpp"
#else
#include "platform_linux.cpp"
#endif

#include "../gen/pre_output.cpp"
#include "compress_search.cpp"
#include "deflate.cpp"
#include "random.cpp"
#include "dorf.cpp"
#include "test_call.cpp"
#include "main.cpp"

