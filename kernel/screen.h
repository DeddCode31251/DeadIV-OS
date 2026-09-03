/* =============================================================================
 * screen.h - VGA text-mode screen driver interface
 * =============================================================================
 */
#ifndef SCREEN_H
#define SCREEN_H

#include "types.h"

/* Standard VGA text-mode colors. You OR a foreground and background
 * together to build a full "attribute byte" (see screen.c for how). */
enum vga_color {
    COLOR_BLACK        = 0,
    COLOR_BLUE         = 1,
    COLOR_GREEN        = 2,
    COLOR_CYAN         = 3,
    COLOR_RED          = 4,
    COLOR_MAGENTA      = 5,
    COLOR_BROWN        = 6,
    COLOR_LIGHT_GREY   = 7,
    COLOR_DARK_GREY    = 8,
    COLOR_LIGHT_BLUE   = 9,
    COLOR_LIGHT_GREEN  = 10,
    COLOR_LIGHT_CYAN   = 11,
    COLOR_LIGHT_RED    = 12,
    COLOR_LIGHT_MAGENTA= 13,
    COLOR_LIGHT_BROWN  = 14,
    COLOR_WHITE        = 15,
};

void screen_clear(void);
void screen_set_color(u8 foreground, u8 background);
void print_char(char c);
void print_string(const char *str);
void print_int(int value);
void print_hex(u32 value);

#endif /* SCREEN_H */
