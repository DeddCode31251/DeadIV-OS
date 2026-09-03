/* =============================================================================
 * string.h - Our own tiny replacement for <string.h>
 * =============================================================================
 * Same story as types.h: no standard library, so we implement the handful
 * of string/memory functions our OS actually needs, ourselves.
 * ===========================================================================
 */
#ifndef STRING_H
#define STRING_H

#include "types.h"

u32  k_strlen(const char *str);
int  k_strcmp(const char *a, const char *b);
void k_memset(void *dest, u8 value, u32 count);
void k_memcpy(void *dest, const void *src, u32 count);
void k_int_to_string(int value, char *out_buffer);

#endif /* STRING_H */
