/* =============================================================================
 * screen.c - VGA text-mode screen driver
 * =============================================================================
 * HOW DOES PRINTING TO THE SCREEN EVEN WORK, WITH NO OS AND NO DRIVERS?
 *
 * When an x86 PC boots, the VGA graphics hardware defaults to "text mode":
 * an 80-column by 25-row grid of character cells. Astonishingly, this whole
 * grid is exposed as a block of ordinary MEMORY starting at the fixed
 * physical address 0xB8000. This is called "memory-mapped I/O" - instead of
 * talking to the device over I/O ports, you just read/write memory, and the
 * VGA hardware watches that address range and updates the physical screen
 * whenever you write to it.
 *
 * Each character cell takes exactly 2 bytes:
 *   byte 0: the ASCII character to display
 *   byte 1: the "attribute byte" - low nibble = foreground color,
 *                                   high nibble = background color
 *
 * So the byte layout for the whole screen looks like:
 *   [char][attr][char][attr][char][attr]...   (80 pairs per row, 25 rows)
 *
 * To print "Hi" in white-on-black at the top-left corner, you'd write:
 *   address 0xB8000 = 'H', 0xB8001 = 0x0F (white on black)
 *   address 0xB8002 = 'i', 0xB8003 = 0x0F
 *
 * That's the ENTIRE mechanism. No drivers, no GPU calls - just memory
 * writes to a fixed address. This file wraps that mechanism in friendly
 * functions like print_string() so the rest of our kernel never has to
 * think about raw VGA memory again.
 * ===========================================================================
 */
#include "screen.h"
#include "ports.h"
#include "string.h"

#define VGA_ADDRESS   0xB8000    /* fixed physical address of video memory */
#define VGA_WIDTH     80         /* columns */
#define VGA_HEIGHT    25         /* rows */

/* A pointer to the start of video memory, treated as an array of 16-bit
 * values (each value = one character cell = char byte + attribute byte). */
static u16 *const vga_buffer = (u16 *) VGA_ADDRESS;

/* Current cursor position and current color, tracked by the driver so
 * print_char() knows where to write next. */
static u32 cursor_row = 0;
static u32 cursor_col = 0;
static u8  current_color = 0x0F; /* default: white text on black background */

/* vga_entry: pack one character + one color attribute into the 16-bit
 * value the VGA hardware expects for a single screen cell. */
static inline u16 vga_entry(char c, u8 color)
{
    return (u16) c | ((u16) color << 8);
}

/* update_hardware_cursor: the blinking cursor block you SEE on screen is
 * a separate feature from where we're currently writing text - it's
 * controlled by the VGA controller chip via two of its I/O ports (this is
 * one of the few places our screen driver needs ports.h). We tell it the
 * cursor's position as a single number: row * 80 + col. */
static void update_hardware_cursor(void)
{
    u16 position = cursor_row * VGA_WIDTH + cursor_col;

    outb(0x3D4, 0x0F);                   /* select "cursor low byte" register */
    outb(0x3D5, (u8) (position & 0xFF));
    outb(0x3D4, 0x0E);                   /* select "cursor high byte" register */
    outb(0x3D5, (u8) ((position >> 8) & 0xFF));
}

/* screen_clear: fill the entire screen with blank (space) characters in
 * the current color, and reset the cursor to the top-left. */
void screen_clear(void)
{
    for (u32 row = 0; row < VGA_HEIGHT; row++) {
        for (u32 col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[row * VGA_WIDTH + col] = vga_entry(' ', current_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hardware_cursor();
}

/* screen_set_color: change the color used for future characters printed.
 * VGA packs foreground (0-15) into the low nibble and background (0-15,
 * though only 0-7 are standard without enabling "blink" tricks) into the
 * high nibble of the attribute byte. */
void screen_set_color(u8 foreground, u8 background)
{
    current_color = foreground | (background << 4);
}

/* scroll_if_needed: if the cursor has gone past the last row, shift every
 * row up by one (copying row 1 into row 0, row 2 into row 1, etc.),
 * clear the new last row, and move the cursor back up. This is what makes
 * the screen "scroll" like a terminal instead of just crashing once full. */
static void scroll_if_needed(void)
{
    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    for (u32 row = 1; row < VGA_HEIGHT; row++) {
        for (u32 col = 0; col < VGA_WIDTH; col++) {
            vga_buffer[(row - 1) * VGA_WIDTH + col] = vga_buffer[row * VGA_WIDTH + col];
        }
    }

    /* Clear the now-duplicated last row */
    for (u32 col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ', current_color);
    }

    cursor_row = VGA_HEIGHT - 1;
}

/* print_char: print one character and advance the cursor, handling the
 * special control characters '\n' (newline) and '\b' (backspace), which
 * every terminal/keyboard-driven shell needs. */
void print_char(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', current_color);
    } else {
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, current_color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    scroll_if_needed();
    update_hardware_cursor();
}

/* print_string: print every character of a null-terminated string. */
void print_string(const char *str)
{
    for (u32 i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

/* print_int: print a signed integer in decimal (we have no printf, so we
 * build this ourselves on top of k_int_to_string from string.c). */
void print_int(int value)
{
    char buffer[12];
    k_int_to_string(value, buffer);
    print_string(buffer);
}

/* print_hex: print an unsigned 32-bit value in hexadecimal, e.g. 0x1A2B3C4D.
 * Useful for debugging - memory addresses and register dumps are much more
 * readable in hex than decimal. */
void print_hex(u32 value)
{
    const char *digits = "0123456789ABCDEF";
    char buffer[11]; /* "0x" + 8 hex digits + null terminator */
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[10] = '\0';

    for (int i = 0; i < 8; i++) {
        u8 nibble = (value >> ((7 - i) * 4)) & 0xF;
        buffer[2 + i] = digits[nibble];
    }

    print_string(buffer);
}
