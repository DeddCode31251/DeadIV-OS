/* =============================================================================
 * keyboard.c - PS/2 keyboard driver
 * =============================================================================
 * HOW DOES THE OS FIND OUT A KEY WAS PRESSED?
 *
 * The (virtual, in our case) PS/2 keyboard controller is wired to IRQ1.
 * When you press or release a key, the controller raises IRQ1, the PIC
 * turns that into CPU interrupt 33 (see idt.c's pic_remap), the CPU jumps
 * to isr_keyboard (idt_load.asm), which calls this file's
 * keyboard_handler_main().
 *
 * Our job here is simple: read one byte - the "scancode" - from I/O port
 * 0x60 (the keyboard controller's data port), and figure out what it
 * means. A "scancode" is NOT an ASCII character - it's a raw hardware
 * code identifying a physical key position, defined by "Scancode Set 1",
 * a standard dating back to the original IBM PC keyboard. We translate
 * scancodes to ASCII ourselves, using a lookup table.
 *
 * Two important quirks of scancode set 1:
 *   - A code with its high bit set (>= 0x80) means "key RELEASED", not
 *     pressed. E.g. pressing 'A' sends 0x1E, releasing it sends 0x9E
 *     (0x1E + 0x80). For this simple shell we only care about presses.
 *   - Special keys (arrows, F-keys, etc.) send a 2-byte sequence starting
 *     with 0xE0. We don't handle those yet - see the README for how you'd
 *     extend this.
 * ===========================================================================
 */
#include "keyboard.h"
#include "ports.h"
#include "screen.h"

/* Forward declaration - implemented in shell.c. Every printable character
 * the user types gets forwarded here. */
extern void shell_handle_char(char c);

#define KEYBOARD_DATA_PORT 0x60

/* Scancode Set 1 -> ASCII lookup table, US QWERTY layout, lowercase/
 * unshifted only (191 entries covers the standard key range; index by
 * raw scancode). A value of 0 means "no printable ASCII for this key"
 * (e.g. Shift, Ctrl, Caps Lock, F-keys - we simply ignore those). */
static const char scancode_to_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',   /* 0x00-0x09 */
    '9', '0', '-', '=', '\b','\t','q', 'w', 'e', 'r',   /* 0x0A-0x13 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,     /* 0x14-0x1D (0x1D=Ctrl) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 0x1E-0x27 */
    '\'','`', 0,   '\\','z', 'x', 'c', 'v', 'b', 'n',    /* 0x28-0x31 (0x2A=LShift) */
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,      /* 0x32-0x3B (space=0x39) */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x3C-0x45 (F-keys etc) */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x46-0x4F */
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,      /* 0x50-0x59 */
    0,   0,   0,                                         /* 0x5A-0x5C */
};

void keyboard_handler_main(void)
{
    u8 scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode & 0x80) {
        /* This is a "key released" event - we ignore these in this simple
         * shell (a fuller OS would use release events to track modifier
         * keys like Shift being held down). */
        return;
    }

    if (scancode >= 128) {
        return; /* out of range of our small table (e.g. an 0xE0 prefix) */
    }

    char ascii = scancode_to_ascii[scancode];
    if (ascii != 0) {
        shell_handle_char(ascii);
    }
}
