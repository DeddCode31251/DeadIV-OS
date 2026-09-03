/* =============================================================================
 * idt.h - Interrupt Descriptor Table interface
 * =============================================================================
 * WHAT IS AN INTERRUPT, AND WHY DO WE NEED A TABLE FOR IT?
 *
 * Hardware devices (keyboard, timer, disk, etc.) don't wait patiently for
 * the CPU to check on them. Instead, when something happens (a key is
 * pressed!) the device electrically signals the CPU: "stop what you're
 * doing right now and come deal with me." This signal is an INTERRUPT.
 *
 * When an interrupt fires, the CPU needs to know exactly which function to
 * jump to for THAT particular interrupt number (keyboard is a different
 * interrupt number than, say, a divide-by-zero error). The IDT is simply
 * an array of 256 entries - one per possible interrupt number 0-255 - each
 * entry saying "if this interrupt fires, here's the memory address of the
 * handler function to run."
 *
 * We tell the CPU where our IDT lives using the LIDT instruction (Load
 * Interrupt Descriptor Table), the same way we used LGDT for the GDT back
 * in boot.asm.
 * ===========================================================================
 */
#ifndef IDT_H
#define IDT_H

#include "types.h"

/* One 8-byte entry in the IDT, describing a single interrupt handler.
 * "packed" tells the compiler not to insert any padding bytes between
 * fields - the CPU expects this EXACT byte layout, down to the byte. */
struct idt_entry {
    u16 offset_low;     /* handler address, bits 0-15 */
    u16 selector;        /* which GDT code segment to run the handler in */
    u8  zero;             /* unused, must be 0 */
    u8  type_attr;        /* type and flags (present bit, ring, gate type) */
    u16 offset_high;      /* handler address, bits 16-31 */
} __attribute__((packed));

/* The structure the LIDT instruction actually loads - size of the table
 * minus 1, and the table's address. Identical idea to gdt_descriptor in
 * boot.asm. */
struct idt_pointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

void idt_install(void);
void idt_set_gate(u8 num, u32 handler_address);

/* keyboard_handler is the C function our assembly IRQ1 stub calls into.
 * Declared here so keyboard.c can be the one that actually implements it,
 * while idt.c wires up the raw interrupt plumbing. */
void keyboard_handler_main(void);

#endif /* IDT_H */
