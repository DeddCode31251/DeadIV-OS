; =============================================================================
; idt_load.asm - Assembly glue that C code cannot do by itself
; =============================================================================
; Two things live here that are impossible to express in plain C:
;   1. idt_load - executing the LIDT instruction.
;   2. isr_keyboard - the actual, literal address the CPU jumps to when a
;      keyboard interrupt fires. This CANNOT be an ordinary C function,
;      because hardware interrupts don't follow the normal C "function
;      call" convention (they don't push a return address the same way,
;      and we must use the special IRET instruction, not RET, to return).
;      So we write a small "trampoline": save every register, call into
;      a normal C function to do the real work, restore every register,
;      tell the PIC we handled it, and IRET back to whatever the CPU was
;      doing before it got interrupted.
; =============================================================================

[BITS 32]

[GLOBAL idt_load]
[GLOBAL isr_keyboard]
[EXTERN keyboard_handler_main]     ; the real C handler, in keyboard.c

; -----------------------------------------------------------------------------
; idt_load(u32 idt_pointer_address)
; -----------------------------------------------------------------------------
; C calling convention (cdecl) passes the single argument on the stack.
; [esp+4] is that argument, because [esp] holds the return address that
; CALL pushed automatically.
idt_load:
    mov eax, [esp+4]      ; eax = idt_pointer_address (the argument)
    lidt [eax]             ; LIDT reads a 6-byte structure from memory (2
                             ; bytes limit + 4 bytes base address) and loads
                             ; it into the CPU's IDTR register - this is
                             ; exactly analogous to LGDT back in boot.asm.
    ret

; -----------------------------------------------------------------------------
; isr_keyboard - the raw interrupt entry point for IRQ1 (keyboard)
; -----------------------------------------------------------------------------
isr_keyboard:
    pushad                  ; push ALL general-purpose registers (eax, ebx,
                             ; ecx, edx, esi, edi, ebp, esp) - the code that
                             ; got interrupted doesn't know it was
                             ; interrupted, so we must leave every register
                             ; exactly as we found it when we're done.

    call keyboard_handler_main   ; do the real work in C - read the
                                   ; scancode, update the shell input buffer

    ; Tell the PIC "I've handled this interrupt" by sending it an
    ; End-Of-Interrupt (EOI) command (0x20) on its command port. Without
    ; this, the PIC assumes we're still busy and will NEVER send us
    ; another interrupt - the keyboard would appear to stop working after
    ; the very first key press!
    mov al, 0x20
    out 0x20, al

    popad                    ; restore every register we saved
    iret                      ; "interrupt return" - unlike a normal RET,
                              ; this also restores the flags register and
                              ; correctly resumes whatever was running
                              ; before the interrupt fired.
