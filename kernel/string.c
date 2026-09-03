/* =============================================================================
 * string.c - Our own tiny replacement for <string.h>
 * =============================================================================
 * Every one of these is a function you've probably used before if you've
 * written any C - strlen, strcmp, memset, memcpy. Here we implement them
 * by hand because we have no standard library to provide them. They're
 * prefixed with k_ (for "kernel") to avoid ever clashing with a real libc
 * if one is ever linked in by mistake.
 * ===========================================================================
 */
#include "string.h"

/* k_strlen: count characters in a null-terminated string, not including
 * the terminating 0 byte itself. */
u32 k_strlen(const char *str)
{
    u32 len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/* k_strcmp: compare two strings character by character.
 * Returns 0 if they're equal.
 * Returns a negative number if 'a' sorts before 'b'.
 * Returns a positive number if 'a' sorts after 'b'.
 * (This matches the behaviour of the standard strcmp.) */
int k_strcmp(const char *a, const char *b)
{
    u32 i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
        i++;
    }
    /* One or both strings ended - compare their final characters (one of
     * which may be the null terminator, which sorts before any letter). */
    return (unsigned char)a[i] - (unsigned char)b[i];
}

/* k_memset: fill 'count' bytes starting at 'dest' with the byte 'value'.
 * Used constantly - e.g. to clear a chunk of memory to zero, or to fill
 * the screen with blank characters. */
void k_memset(void *dest, u8 value, u32 count)
{
    u8 *d = (u8 *)dest;
    for (u32 i = 0; i < count; i++) {
        d[i] = value;
    }
}

/* k_memcpy: copy 'count' bytes from 'src' to 'dest'. */
void k_memcpy(void *dest, const void *src, u32 count)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    for (u32 i = 0; i < count; i++) {
        d[i] = s[i];
    }
}

/* k_int_to_string: convert a signed integer into a decimal string.
 * Handy for printing numbers to the screen (we have no printf!).
 * 'out_buffer' must be large enough to hold the result (12 bytes is safe
 * for any 32-bit int including the sign and null terminator). */
void k_int_to_string(int value, char *out_buffer)
{
    int i = 0;
    bool is_negative = false;

    if (value == 0) {
        out_buffer[0] = '0';
        out_buffer[1] = '\0';
        return;
    }

    if (value < 0) {
        is_negative = true;
        value = -value;
    }

    /* Peel off digits from least-significant to most-significant - this
     * naturally produces them in REVERSE order, so we reverse afterwards. */
    char temp[12];
    int temp_len = 0;
    while (value != 0) {
        temp[temp_len++] = (value % 10) + '0';
        value /= 10;
    }

    if (is_negative) {
        out_buffer[i++] = '-';
    }

    for (int j = temp_len - 1; j >= 0; j--) {
        out_buffer[i++] = temp[j];
    }

    out_buffer[i] = '\0';
}
