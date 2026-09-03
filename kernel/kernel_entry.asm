; =============================================================================
; kernel_entry.asm - The very first bytes of the kernel binary
; =============================================================================
; Remember from boot.asm: after switching to protected mode, the bootloader
; does "jmp KERNEL_LOAD_ADDR" (jumps to address 0x1000). That means whatever
; bytes we place at the VERY START of our linked kernel binary are the first
; instructions to run. This tiny file's only job is to be that first
; instruction, and to call our real entry point: the C function kmain().
;
; Why can't the bootloader just call C code directly? It can - but the
; linker needs to control WHERE in the final kernel.bin this "jump to C"
; code lives, which is why we make it its own object file and list it FIRST
; when linking (see linker.ld / Makefile). This guarantees it ends up at
; offset 0 in the final binary, i.e. exactly at address 0x1000 in memory.
; =============================================================================

[BITS 32]                     ; we arrive here already in 32-bit protected
                               ; mode (the bootloader switched modes for us)

[EXTERN kmain]                 ; "extern" tells NASM: kmain is defined
                                ; elsewhere (in kernel.c) - just trust that
                                ; the linker will resolve the address later.

[GLOBAL start]                 ; "global" makes the label "start" visible to
                                ; the linker, so the linker script can point
                                ; the entry point at it.

start:
    call kmain                  ; jump into our C code! From here on,
                                 ; everything is written in C and is much
                                 ; easier to read and extend.

    ; kmain() should never return (it ends in an infinite loop), but as a
    ; safety net, if it somehow does return, halt the CPU forever instead
    ; of running off into garbage memory and crashing unpredictably.
.hang:
    cli                          ; disable interrupts
    hlt                          ; halt the CPU until the next interrupt
    jmp .hang                    ; if an interrupt does wake us, halt again
