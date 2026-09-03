# =============================================================================
# Makefile - builds DeadIV OS with a single command: `make`
# =============================================================================
# `make` reads this file and figures out, from the dependency rules below,
# exactly which commands need to run and in what order to produce the
# final os-image.bin. If you edit one .c file and run `make` again, it's
# smart enough to only recompile that file, not the whole project.
# =============================================================================

# Tools we installed earlier
ASM = nasm
CC  = gcc
LD  = ld

# Compiler flags, explained:
#   -m32               compile 32-bit code (our OS is 32-bit protected mode)
#   -ffreestanding      tells GCC "there is no standard library / hosted
#                       environment; don't assume main() exists, don't
#                       assume libc functions exist"
#   -fno-pie -fno-stack-protector -fno-builtin
#                       disable OS-assuming features: position-independent
#                       code needs a dynamic linker (we have none), the
#                       stack protector needs libc support functions we
#                       don't have, and "builtins" are cases where GCC
#                       silently calls libc functions like memcpy behind
#                       your back - not allowed here.
#   -nostdlib           don't link against the C standard library at all
#   -Wall -Wextra       show all warnings - very useful when writing OS
#                       code, where bugs can be catastrophic
#   -O0                 no optimization, which makes debugging far easier
#                       (crank this to -O2 once your OS is stable)
CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
         -nostdlib -Wall -Wextra -O0 -g

# Every .c file in kernel/ becomes a .o file in build/
C_SOURCES = $(wildcard kernel/*.c)
C_OBJECTS = $(patsubst kernel/%.c, build/%.o, $(C_SOURCES))

.PHONY: all clean run run-debug

# `make` or `make all` builds the final bootable disk image
all: build/os-image.bin

# ---------------------------------------------------------------------------
# Assemble the boot sector to a flat 512-byte binary (no ELF headers - the
# BIOS wants raw machine code starting at byte 0).
# ---------------------------------------------------------------------------
build/boot.bin: boot/boot.asm
	$(ASM) -f bin $< -o $@

# ---------------------------------------------------------------------------
# Assemble the kernel entry stub and IDT assembly glue as ELF object files,
# so the linker can combine them with our compiled C object files.
# ---------------------------------------------------------------------------
build/kernel_entry.o: kernel/kernel_entry.asm
	$(ASM) -f elf32 $< -o $@

build/idt_load.o: kernel/idt_load.asm
	$(ASM) -f elf32 $< -o $@

# ---------------------------------------------------------------------------
# Compile every C file into an object file.
# This is a "pattern rule": it applies to any build/X.o built from kernel/X.c
# ---------------------------------------------------------------------------
build/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# Link everything into one ELF kernel binary, using our custom linker
# script (linker.ld) to control where it lives in memory.
#
# IMPORTANT: kernel_entry.o is listed FIRST on the command line, which
# guarantees its code (the "start" label) ends up at the very beginning of
# the .text section - i.e. exactly at address 0x1000, exactly where the
# bootloader jumps to.
# ---------------------------------------------------------------------------
build/kernel.elf: build/kernel_entry.o build/idt_load.o $(C_OBJECTS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o build/kernel.elf \
		build/kernel_entry.o build/idt_load.o $(C_OBJECTS)

# ---------------------------------------------------------------------------
# Strip the ELF metadata away, leaving pure raw machine code/data - this is
# what actually gets loaded into memory by our bootloader, which has no
# idea what an "ELF file" even is.
# ---------------------------------------------------------------------------
build/kernel.bin: build/kernel.elf
	objcopy -O binary build/kernel.elf build/kernel.bin

# ---------------------------------------------------------------------------
# Glue the boot sector and the kernel binary together into one disk image:
# byte 0-511 is the boot sector, byte 512 onward is the kernel, exactly as
# our bootloader's disk-read code expects (kernel starts at "sector 2").
# ---------------------------------------------------------------------------
build/os-image.bin: build/boot.bin build/kernel.bin
	cat build/boot.bin build/kernel.bin > build/os-image.bin
	# Pad the image to a standard 1.44 MB floppy disk size (2880 sectors
	# of 512 bytes). This matters! The BIOS's INT 0x13 disk-read call
	# expects to be able to read KERNEL_SECTORS sectors starting at
	# sector 2 - if the file itself is shorter than that, the read can
	# fail or hang. Padding to a real floppy size guarantees there's
	# always enough room, no matter how large the kernel grows (up to
	# the 1.44 MB limit).
	truncate -s 1474560 build/os-image.bin
	@echo ""
	@echo "Build complete: build/os-image.bin"
	@echo "Run it with:    make run"

# ---------------------------------------------------------------------------
# Boot the finished image in the QEMU emulator - a full virtual PC, so you
# never have to risk your real computer while developing an OS.
# ---------------------------------------------------------------------------
run: build/os-image.bin
	qemu-system-i386 -fda build/os-image.bin

# Same, but pauses at startup and waits for a debugger (gdb) to attach on
# port 1234 - see the README's debugging section.
run-debug: build/os-image.bin
	qemu-system-i386 -fda build/os-image.bin -s -S

clean:
	rm -rf build/*.o build/*.bin build/*.elf
