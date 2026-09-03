/* =============================================================================
 * idt.c - Building the Interrupt Descriptor Table and remapping the PIC
 * =============================================================================
 * This file has two jobs:
 *
 *   1. Build the 256-entry IDT array and load it with LIDT (via a small
 *      helper written in idt_load.asm, since C can't execute LIDT itself).
 *
 *   2. "Remap the PIC". The PIC (Programmable Interrupt Controller) is the
 *      old chip that takes hardware signals from devices (keyboard, timer,
 *      etc, called "IRQs" 0-15) and turns them into CPU interrupts. By
 *      default, on boot, IT USES INTERRUPT NUMBERS 0-15 - which COLLIDE
 *      with the CPU's own reserved interrupts 0-15 (things like "divide by
 *      zero" and "page fault"). Every real x86 OS must reprogram the PIC
 *      at startup to use a different, non-colliding range of interrupt
 *      numbers instead (we choose 32-47, right after the CPU's reserved
 *      range). This "remapping" is done by sending a specific sequence of
 *      bytes to two I/O ports the PIC listens on.
 * ===========================================================================
 */
#include "idt.h"
#include "ports.h"
#include "string.h"

/* extern declarations for the assembly helpers in idt_load.asm */
extern void idt_load(u32 idt_pointer_address);
extern void isr_keyboard(void); /* the raw asm entry point for IRQ1 */

static struct idt_entry idt[256];
static struct idt_pointer idt_ptr;

/* idt_set_gate: fill in one IDT entry.
 * num              - which interrupt number (0-255)
 * handler_address  - address of the handler function/stub to run */
void idt_set_gate(u8 num, u32 handler_address)
{
    idt[num].offset_low  = handler_address & 0xFFFF;
    idt[num].offset_high = (handler_address >> 16) & 0xFFFF;
    idt[num].selector    = 0x08;   /* our kernel code segment, selector 0x08,
                                     matching CODE_SEG from boot.asm's GDT */
    idt[num].zero        = 0;
    idt[num].type_attr   = 0x8E;   /* present=1, ring=00, type=0xE
                                     ("32-bit interrupt gate") */
}

/* pic_remap: reprogram the two 8259 PIC chips (a "master" handling IRQs
 * 0-7, and a "slave" handling IRQs 8-15) to fire CPU interrupts 32-47
 * instead of the default, colliding 0-15. Each PIC is controlled by
 * writing a specific 4-byte sequence called "ICW1-ICW4" (Initialization
 * Command Words) to its command and data I/O ports. This sequence is
 * standard PC hardware boilerplate - every hobby OS has to do this. */
static void pic_remap(void)
{
    /* ICW1: start initialization sequence on both PICs */
    outb(0x20, 0x11);   /* master PIC command port */
    outb(0xA0, 0x11);   /* slave PIC command port */

    /* ICW2: set the interrupt vector offsets - where each PIC's IRQs
     * should start in the CPU's interrupt number space */
    outb(0x21, 0x20);   /* master PIC: IRQ 0-7  -> interrupts 32-39 */
    outb(0xA1, 0x28);   /* slave PIC:  IRQ 8-15 -> interrupts 40-47 */

    /* ICW3: tell the PICs how they're wired to each other (cascade) */
    outb(0x21, 0x04);   /* tell master PIC there's a slave at IRQ2 */
    outb(0xA1, 0x02);   /* tell slave PIC its own cascade identity */

    /* ICW4: set 8086/88 mode (normal x86 mode) */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Mask (disable) all IRQ lines except IRQ1 (keyboard) for now - we
     * haven't written handlers for the timer, mouse, disk, etc. yet, and
     * an unhandled interrupt would crash the machine. 0xFD = binary
     * 11111101 - every bit is 1 (masked/disabled) except bit 1 (IRQ1). */
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

/* idt_install: build the table and load it into the CPU. */
void idt_install(void)
{
    idt_ptr.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_ptr.base  = (u32) &idt;

    /* Start with every entry zeroed (i.e. "not present" - if a stray
     * interrupt fires with no handler, the CPU will raise a fault we can
     * at least see, rather than jumping to garbage memory). */
    k_memset(&idt, 0, sizeof(struct idt_entry) * 256);

    pic_remap();

    /* Wire up interrupt number 33 (32 + IRQ1) to our keyboard stub.
     * Remember from pic_remap: IRQ1 now arrives as CPU interrupt 33. */
    idt_set_gate(33, (u32) isr_keyboard);

    idt_load((u32) &idt_ptr);   /* actually execute LIDT via our asm helper */

    __asm__ __volatile__ ("sti"); /* Set Interrupt flag - re-enable hardware
                                      interrupts now that our table is safely
                                      installed. Before this point, an
                                      incoming interrupt would have had
                                      nowhere valid to go. */
}
