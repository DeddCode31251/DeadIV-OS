/* =============================================================================
 * ports.h - Talking directly to hardware over the x86 "I/O bus"
 * =============================================================================
 * Unlike memory, x86 CPUs have a completely separate address space called
 * "I/O ports" (65536 of them, numbered 0x0000-0xFFFF). Old-school hardware
 * devices - the keyboard controller, the PIC (interrupt controller), disk
 * controllers, etc. - are wired up to specific port numbers instead of
 * memory addresses. To talk to them, the CPU has two special instructions:
 *
 *   outb port, value   -> write one byte to an I/O port
 *   inb  port          -> read one byte from an I/O port
 *
 * C has no built-in way to execute these, so we write tiny "inline
 * assembly" wrapper functions. Every hardware driver in this OS (screen
 * cursor control, keyboard, PIC) goes through these two functions.
 * ===========================================================================
 */
#ifndef PORTS_H
#define PORTS_H

#include "types.h"

/* outb: send a byte 'value' to I/O port 'port'.
 *
 * __asm__ __volatile__ ( ... ) embeds raw assembly inside C.
 *   "outb %0, %1"   - the actual x86 instruction, AT&T syntax
 *   : (no outputs)
 *   : "a" (value), "Nd" (port)   - inputs: put 'value' in register AL
 *                                  (the "a" constraint), and 'port' in DX
 *                                  ("d") or as an immediate if it's a small
 *                                  constant ("N")
 * "volatile" tells the compiler "don't optimize this away or reorder it -
 * it has a side effect the compiler can't see (talking to hardware)".
 */
static inline void outb(u16 port, u8 value)
{
    __asm__ __volatile__ ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* inb: read one byte from I/O port 'port' and return it. */
static inline u8 inb(u16 port)
{
    u8 result;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

#endif /* PORTS_H */
