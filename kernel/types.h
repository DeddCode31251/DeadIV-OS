/* =============================================================================
 * types.h - Our own fixed-size integer types
 * =============================================================================
 * Normally in C you'd `#include <stdint.h>` to get types like uint8_t and
 * uint32_t. But <stdint.h> is part of the C STANDARD LIBRARY, and we are
 * building "freestanding" - we have NO standard library, because the
 * standard library assumes an operating system underneath it to provide
 * things like malloc() (which needs virtual memory) and printf() (which
 * needs a screen/file driver). We ARE the operating system, so we build
 * these primitives ourselves, from nothing.
 *
 * Plain `int` and `char` in C do not have a guaranteed size (it varies by
 * compiler/platform!) which is unacceptable when you're writing to exact
 * hardware registers and memory-mapped structures. So we define our own
 * short, exact-width names.
 * ===========================================================================
 */
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char      u8;   /* exactly 1 byte  (0 to 255) */
typedef unsigned short     u16;  /* exactly 2 bytes (0 to 65535) */
typedef unsigned int       u32;  /* exactly 4 bytes (0 to ~4 billion) */
typedef unsigned long long u64;  /* exactly 8 bytes */

typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;

/* We don't have C's <stdbool.h> either, so we make our own tiny boolean.
 *
 * NOTE: newer compilers (GCC 13+ defaulting to the C23 standard) already
 * treat `bool`, `true`, and `false` as built-in keywords, the same way
 * C++ does. If we blindly `typedef u8 bool;` on such a compiler, it
 * collides with the keyword and fails with an error like:
 *   "two or more data types in declaration specifiers"
 * __STDC_VERSION__ is a value the compiler defines to tell us which C
 * standard it's using; C23 is numbered 202311L. We only define our own
 * bool if the compiler is OLDER than C23 (or doesn't define the macro at
 * all, meaning a very old/strict compiler) and hasn't defined it for us
 * already. */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
typedef u8 bool;
#define true 1
#define false 0
#endif

/* NULL, the "points at nothing" pointer value. */
#define NULL ((void*)0)

#endif /* TYPES_H */
