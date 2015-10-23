
#include "prelude.h"

#include "platform_shared.cpp"
#ifdef _WIN32
#include "platform_windows.cpp"
#else
#include "platform_linux.cpp"
#endif

#include "../gen/pre_output.cpp"
#include "source_loc.cpp"
#include "debug_alloc.cpp"
#include "strings.cpp"
#include "utf.cpp"
#include "memory.cpp"
#include "string_table.cpp"
#include "scanner.cpp"
#include "printer.cpp"
#include "xml.cpp"
#include "svg.cpp"
#include "gzip/compress_search.cpp"
#include "gzip/deflate.cpp"
#include "random.cpp"
#include "assets.cpp"
#include "dorf.cpp"
#include "test_call.cpp"
#include "main.cpp"

