/* =============================================================================
 * keyboard.h - PS/2 keyboard driver interface
 * =============================================================================
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

/* keyboard_handler_main is called (indirectly, via isr_keyboard in
 * idt_load.asm) every single time a key is pressed or released. It reads
 * the raw scancode from the keyboard controller and, on a key press,
 * hands the translated ASCII character off to the shell. */
void keyboard_handler_main(void);

#endif /* KEYBOARD_H */
