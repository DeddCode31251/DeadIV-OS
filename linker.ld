/* =============================================================================
 * linker.ld - Tells the linker how to arrange our kernel in memory
 * =============================================================================
 * A compiler turns each .c/.asm file into an "object file" (.o) - a chunk of
 * machine code plus a list of things it needs from elsewhere ("undefined
 * symbols" like kmain) and things it provides ("defined symbols"). The
 * LINKER's job is to glue all these object files into one final binary,
 * resolving every symbol reference and deciding the final memory addresses
 * of everything.
 *
 * A linker script is a small program written in the linker's own language
 * that gives explicit instructions instead of the linker's built-in
 * defaults, which we can't rely on for a bare-metal OS.
 * ===========================================================================
 */

/* ENTRY tells the linker (and any tool inspecting the file) which symbol is
 * the program's starting point. We defined "start" in kernel_entry.asm. */
ENTRY(start)

SECTIONS
{
    /* Our bootloader loads the kernel to physical address 0x1000 (see
     * KERNEL_LOAD_ADDR in boot.asm) and jumps straight there with no
     * relocation. So we tell the linker: "pretend/arrange that address 0
     * of this output corresponds to memory address 0x1000." Every address
     * the compiler bakes into the code (for global variables, function
     * calls, etc.) will be computed relative to this base. */
    . = 0x1000;

    /* .text holds actual CPU instructions (compiled code). We explicitly
     * put kernel_entry.o's contents FIRST with *(.text.entry) below is not
     * used here; instead we rely on Makefile listing kernel_entry.o first
     * on the ld command line, which places it first within .text. */
    .text : ALIGN(4)
    {
        *(.text)
    }

    /* .rodata holds "read-only data" - things like string literals
     * ("Hello, world" in C compiles into bytes stored here). */
    .rodata : ALIGN(4)
    {
        *(.rodata)
    }

    /* .data holds initialized global/static variables (e.g. int x = 5;) */
    .data : ALIGN(4)
    {
        *(.data)
    }

    /* .bss holds uninitialized global/static variables (e.g. int y;).
     * These don't need to take up space in the binary file itself - the
     * linker just remembers "reserve this many zeroed bytes at runtime" -
     * which keeps our kernel.bin file smaller. Our kernel code must zero
     * this region itself at startup if it relies on C's "globals start at
     * zero" guarantee (see kernel.c). */
    .bss : ALIGN(4)
    {
        *(.bss)
    }
}
