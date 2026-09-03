; =============================================================================
; boot.asm - The DeadIV OS Bootloader (Stage 1)
; =============================================================================
;
; WHAT IS THIS FILE?
; When you turn on a PC, the BIOS (firmware built into the motherboard) does
; a self-test, then looks at the first storage device it's told to boot from
; (a floppy image, USB stick, hard disk, whatever). It reads the very FIRST
; 512 bytes of that device into memory at address 0x7C00, and if the last two
; bytes of that 512-byte block are the magic numbers 0x55 0xAA, the BIOS
; assumes "this is a valid boot sector" and jumps to address 0x7C00 to start
; executing it.
;
; That's it. No operating system, no file system driver, nothing. Just raw
; CPU instructions in a 512-byte box. This file IS that 512-byte box.
;
; The CPU starts in "16-bit real mode" — an ancient compatibility mode from
; the 1970s Intel 8086 chip, where you can only address 1 MB of memory and
; there's no memory protection at all. Our job in this file is:
;   1. Set up a stack so we can call functions.
;   2. Print a message so we know we're alive.
;   3. Ask the BIOS (via a disk interrupt) to load our real kernel from disk
;      into memory, because our kernel is way bigger than 512 bytes.
;   4. Switch the CPU from 16-bit real mode into 32-bit "protected mode" —
;      the mode every real OS actually runs in, which gives us 4 GB of
;      addressable memory and (eventually) memory protection.
;   5. Jump into the kernel and never come back.
;
; =============================================================================

; [BITS 16] tells NASM (our assembler) to generate 16-bit instructions,
; because that's the only mode the CPU understands right at boot.
[BITS 16]

; [ORG 0x7C00] tells NASM "assume this code will be loaded at memory address
; 0x7C00". This matters because some instructions embed absolute addresses,
; and NASM needs to compute them correctly.
[ORG 0x7C00]

; -----------------------------------------------------------------------
; ENTRY POINT
; -----------------------------------------------------------------------
start:
    ; The BIOS may leave segment registers in an unknown state. We clear
    ; them to a known value (0) so our memory addressing is predictable.
    ; On x86, a real-mode address is computed as (segment * 16) + offset.
    cli                     ; "Clear Interrupts" - disable hardware interrupts
                            ; while we set up the stack, so we can't be
                            ; interrupted mid-setup and crash.
    xor ax, ax              ; ax = 0  (xor-ing a register with itself is the
                            ; classic fast way to zero it on x86)
    mov ds, ax              ; Data Segment = 0
    mov es, ax              ; Extra Segment = 0
    mov ss, ax              ; Stack Segment = 0
    mov sp, 0x7C00          ; Stack Pointer = 0x7C00. The stack grows
                            ; DOWNWARD in memory, and our boot sector code
                            ; lives AT 0x7C00 upward, so placing the stack
                            ; just below it means it grows away from our
                            ; code and won't overwrite it.
    sti                     ; re-enable interrupts now that we're set up

    ; Save which drive we booted from. The BIOS is kind enough to put the
    ; "boot drive number" in the DL register before jumping to us (e.g.
    ; 0x00 = floppy, 0x80 = first hard disk). We'll need this later to tell
    ; the BIOS which drive to read the kernel from.
    mov [BOOT_DRIVE], dl

    ; Print a message so a human watching the screen knows the bootloader
    ; is alive and running. See the print_string routine below.
    mov si, MSG_REAL_MODE
    call print_string

    ; -----------------------------------------------------------------------
    ; STEP 1: Load the kernel from disk into memory
    ; -----------------------------------------------------------------------
    ; Our kernel binary sits on disk right after this boot sector (starting
    ; at "sector 2" - sectors are numbered from 1, and sector 1 is us).
    ; We ask the BIOS to read it into memory at address 0x1000 (= 4096),
    ; which is comfortably above our bootloader and below the 1MB real-mode
    ; ceiling.
    call load_kernel

    ; -----------------------------------------------------------------------
    ; STEP 2: Switch to 32-bit Protected Mode
    ; -----------------------------------------------------------------------
    call switch_to_protected_mode

    ; switch_to_protected_mode never returns - it jumps straight into
    ; protected-mode code below and eventually into the kernel. So execution
    ; never actually reaches this point, but just in case, halt forever.
    jmp $

; =============================================================================
; print_string - Print a null-terminated string in 16-bit real mode
; =============================================================================
; Input: SI = pointer to a string ending in a 0 byte
;
; We use BIOS interrupt 0x10, function 0x0E ("teletype output"): it prints
; one character to the screen and advances the cursor automatically, exactly
; like a very old-school printf("%c"). We don't have printf yet - the BIOS
; IS our print function until the kernel writes its own screen driver.
; -----------------------------------------------------------------------------
print_string:
    pusha                   ; push all general registers (save them, since
                            ; we're about to clobber ax/bx)
.loop:
    lodsb                   ; load the byte at [SI] into AL, and increment SI
                            ; automatically (lodsb = "load string byte")
    cmp al, 0                ; is this the null terminator?
    je .done                 ; if so, we're finished
    mov ah, 0x0E             ; BIOS function 0x0E = "print character"
    mov bh, 0                ; page number 0
    mov bl, 0x07              ; text color (light grey on black)
    int 0x10                  ; call the BIOS video interrupt
    jmp .loop
.done:
    popa                     ; restore registers
    ret

; =============================================================================
; load_kernel - Use the BIOS disk service to read the kernel off disk
; =============================================================================
; We use BIOS interrupt 0x13, function 0x02 ("read sectors from drive").
; This is old-school CHS (Cylinder/Head/Sector) disk addressing - it's
; primitive, but every BIOS supports it and it's more than good enough for
; loading a small early-stage kernel.
; -----------------------------------------------------------------------------
load_kernel:
    mov si, MSG_LOAD_KERNEL
    call print_string

    mov ah, 0x02             ; BIOS function 0x02 = "read sectors"
    mov al, KERNEL_SECTORS   ; number of sectors to read (defined below)
    mov ch, 0x00              ; cylinder 0
    mov dh, 0x00              ; head 0
    mov cl, 0x02              ; start reading from sector 2 (sector 1 is us,
                              ; the boot sector itself)
    mov dl, [BOOT_DRIVE]      ; which physical drive to read from
    mov bx, KERNEL_LOAD_ADDR  ; ES:BX = destination buffer. ES is already 0
                              ; from our setup above, so this reads to
                              ; physical address 0x0000:0x1000 = 0x1000
    int 0x13                  ; call the BIOS disk interrupt

    jc disk_error              ; the BIOS sets the Carry Flag on error

    ; Sanity check: BIOS also returns the number of sectors actually read
    ; in AL. If it doesn't match what we asked for, something went wrong.
    cmp al, KERNEL_SECTORS
    jne disk_error

    mov si, MSG_LOAD_OK
    call print_string
    ret

disk_error:
    mov si, MSG_DISK_ERROR
    call print_string
    jmp $                     ; halt forever - we can't continue without
                              ; a kernel to run

; =============================================================================
; switch_to_protected_mode - The 16-bit -> 32-bit transition
; =============================================================================
; This is the trickiest part of writing an OS from scratch, so read the
; comments carefully. Real mode has no memory protection and only 20 address
; lines (1 MB max). Protected mode gives us 32-bit addressing (4 GB), memory
; protection rings, and is what every modern kernel actually runs in
; (even 64-bit "long mode" is entered by first passing through protected
; mode).
;
; To enter protected mode we must:
;   1. Disable interrupts (the old real-mode interrupt table becomes
;      meaningless in protected mode until we build a new one).
;   2. Load a Global Descriptor Table (GDT) - a small table describing
;      memory "segments" the CPU is allowed to use. We define a very
;      simple "flat model" GDT: one code segment and one data segment,
;      each spanning the ENTIRE 4 GB address space. This effectively
;      makes segmentation a formality and lets us treat memory as one
;      flat block, like modern software expects.
;   3. Set bit 0 (the "Protection Enable" bit) of the CR0 control register.
;      This one bit flip is what actually switches the CPU into protected
;      mode.
;   4. Do a "far jump" (a jump that also reloads the code segment register)
;      to flush the CPU's instruction prefetch queue, which may still
;      contain 16-bit-decoded instructions, and to officially start
;      executing as 32-bit code.
; -----------------------------------------------------------------------------
switch_to_protected_mode:
    cli                        ; interrupts must stay off until the kernel
                               ; sets up its own Interrupt Descriptor Table

    lgdt [gdt_descriptor]      ; load our GDT (defined below) into the
                               ; CPU's GDTR register

    mov eax, cr0               ; read the CR0 control register
    or eax, 0x1                ; set bit 0 = Protection Enable
    mov cr0, eax               ; write it back - WE ARE NOW IN PROTECTED MODE

    ; A "far jump" specifies both a segment selector and an offset. Jumping
    ; to CODE_SEG:protected_mode_start reloads CS with our new 32-bit code
    ; segment selector and flushes the pipeline.
    jmp CODE_SEG:protected_mode_start

; =============================================================================
; GDT (Global Descriptor Table) - our "flat model" memory map
; =============================================================================
; Each GDT entry ("descriptor") is 8 bytes describing a memory segment: its
; base address, its size limit, and access permissions. We define three
; entries: a mandatory null descriptor, a code segment, and a data segment.
; Both the code and data segments start at base 0x00000000 and cover the
; full 4 GB (limit 0xFFFFF with 4KB granularity = 0xFFFFF * 4KB = 4GB),
; so in practice they overlap completely and segmentation becomes a
; no-op - we just use flat 32-bit addresses everywhere, like a normal
; programming language would expect.
; -----------------------------------------------------------------------------
gdt_start:

gdt_null:                      ; the CPU requires the very first descriptor
    dd 0x0                     ; to be all zero - it's never used, and
    dd 0x0                     ; catches "you forgot to set a segment
                               ; register" bugs.

gdt_code:                      ; code segment descriptor
    dw 0xFFFF                  ; limit (bits 0-15)
    dw 0x0                     ; base (bits 0-15)
    db 0x0                     ; base (bits 16-23)
    db 10011010b               ; access byte:
                                ;   present=1, ring=00 (kernel), type=1 (code/data)
                                ;   executable=1, direction=0, readable=1, accessed=0
    db 11001111b                ; flags + limit(bits 16-19):
                                ;   granularity=1 (limit is in 4KB blocks),
                                ;   32-bit=1 (this is 32-bit protected mode code)
    db 0x0                      ; base (bits 24-31)

gdt_data:                       ; data segment descriptor (identical except
    dw 0xFFFF                   ; the "executable" bit is 0, since you can't
    dw 0x0                      ; execute a data segment)
    db 0x0
    db 10010010b                ; present=1, ring=00, type=1, executable=0,
                                 ; direction=0 (grows up), writable=1
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:                 ; this is the structure the LGDT instruction
    dw gdt_end - gdt_start - 1  ; actually loads: size of the GDT minus 1...
    dd gdt_start                ; ...and the linear address of the GDT

; Segment selectors are just offsets into the GDT table. Each entry is 8
; bytes, so selector 0x08 = the 2nd entry (gdt_code), and 0x10 = the 3rd
; entry (gdt_data). These constants make the rest of the code readable.
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; =============================================================================
; 32-bit protected-mode code starts here
; =============================================================================
[BITS 32]                       ; from this point on, generate 32-bit
                                 ; instructions - we really are in protected
                                 ; mode now.

protected_mode_start:
    ; Reload ALL data-related segment registers with our flat data segment
    ; selector. In real mode, DS/ES/SS held real-mode segment values; now
    ; they must hold GDT selectors instead.
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000             ; set up a fresh 32-bit stack, well above
    mov esp, ebp                 ; our loaded kernel, with plenty of room

    ; Finally: jump into the kernel code we loaded from disk earlier.
    ; KERNEL_LOAD_ADDR is where the BIOS put it (0x1000). This is a genuine
    ; jump, not a call - the bootloader's job ends here forever.
    jmp KERNEL_LOAD_ADDR

; =============================================================================
; Data / constants
; =============================================================================
BOOT_DRIVE       db 0
KERNEL_LOAD_ADDR equ 0x1000
KERNEL_SECTORS   equ 32          ; how many 512-byte disk sectors to load for
                                  ; the kernel (32 sectors = 16 KB). If your
                                  ; kernel ever grows past this, bump this
                                  ; number - see the README for how to
                                  ; calculate the exact value.

MSG_REAL_MODE   db "DeadIV OS bootloader: 16-bit real mode OK", 13, 10, 0
MSG_LOAD_KERNEL db "Loading kernel from disk...", 13, 10, 0
MSG_LOAD_OK     db "Kernel loaded OK. Entering 32-bit protected mode...", 13, 10, 0
MSG_DISK_ERROR  db "DISK READ ERROR - halting.", 13, 10, 0

; =============================================================================
; Boot sector padding and magic number
; =============================================================================
; A boot sector MUST be exactly 512 bytes, and MUST end with the two magic
; bytes 0x55 0xAA or the BIOS will refuse to treat it as bootable.
;
; "$-$$" means "current address minus the address of the start of this
; section" - i.e. how many bytes we've assembled so far. We pad with zero
; bytes until we hit byte 510, then write the two magic bytes at 510-511.
; -----------------------------------------------------------------------------
times 510-($-$$) db 0
dw 0xAA55
