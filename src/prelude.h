#ifndef PRELUDE_H
#define PRELUDE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef int32_t I32;
typedef uint32_t U32;
typedef int64_t I64;
typedef uint64_t U64;
#define Count(array) (sizeof(array)/sizeof(*(array)))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

#endif

