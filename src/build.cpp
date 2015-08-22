
#include "prelude.h"

#ifdef _WIN32
#include "platform_windows.cpp"
#else
#include "platform_linux.cpp"
#endif

#include "../gen/pre_output.cpp"
#include "strings.cpp"
#include "utf.cpp"
#include "memory.cpp"
#include "string_table.cpp"
#include "scanner.cpp"
#include "xml.cpp"
#include "gzip/compress_search.cpp"
#include "gzip/deflate.cpp"
#include "random.cpp"
#include "dorf.cpp"
#include "test_call.cpp"
#include "main.cpp"

