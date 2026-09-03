/* =============================================================================
 * kernel.c - kmain(): the true "main()" of DeadIV OS
 * =============================================================================
 * This is where control finally arrives after:
 *   BIOS -> boot.asm (16-bit) -> protected mode switch -> kernel_entry.asm
 *   -> HERE.
 *
 * Everything from this point on is ordinary, readable C. kmain()'s job is
 * simply to initialize every subsystem in the right order, print a
 * welcome banner, and then hand control over to the shell / interrupt
 * system for the rest of the OS's life.
 * ===========================================================================
 */
#include "screen.h"
#include "idt.h"
#include "shell.h"

/* kmain: called from kernel_entry.asm's "call kmain" instruction. */
void kmain(void)
{
    /* Step 1: clear the garbage/BIOS text off the screen so we start with
     * a clean slate. */
    screen_clear();

    /* Step 2: build and load the Interrupt Descriptor Table, and remap
     * the PIC. Until this runs, hardware interrupts (like key presses)
     * have nowhere valid to go and would crash the machine - this is why
     * it must happen before we ever wait for keyboard input. */
    idt_install();

    /* Step 3: print the banner and first shell prompt. From here on, the
     * OS is entirely event-driven: the CPU sits idle (see the loop below)
     * until an interrupt - right now, only the keyboard - wakes it up,
     * runs a tiny handler, and goes back to idling. */
    shell_init();

    /* Step 4: the "idle loop". We have no other tasks to run (no
     * multitasking - see the README for how that's normally added), so
     * we simply halt the CPU forever. `hlt` stops the CPU from executing
     * any more instructions until the NEXT hardware interrupt arrives,
     * which is far more power-efficient than spinning in an empty
     * "while(1);" loop burning 100% CPU for no reason. Every time a key
     * is pressed, the CPU wakes up just long enough to run
     * isr_keyboard -> keyboard_handler_main -> shell_handle_char, then
     * comes right back here and halts again. */
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}
