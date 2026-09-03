# DeadIV OS

A tiny x86 operating system built from absolute zero — no tutorials copy-pasted blindly, every line explained. This README assumes you know **basic C syntax** (variables, if/else, functions) and **nothing about assembly or operating systems**. By the end, you should understand every file in this project well enough to modify it or write your own OS from scratch.

Tested and confirmed working in QEMU: it boots, prints a banner, and gives you a live interactive shell.

---

<img width="772" height="515" alt="Screenshot From 2026-09-03 12-44-23" src="https://github.com/user-attachments/assets/12d47d6a-4aad-411c-8604-9add6252baee" />


## Table of contents

1. [What actually IS an operating system](#1-what-actually-is-an-operating-system)
2. [What you need installed](#2-what-you-need-installed)
3. [Quick start](#3-quick-start)
4. [The boot process, from power button to shell](#4-the-boot-process-from-power-button-to-shell)
5. [Project file map](#5-project-file-map)
6. [Deep dive: the bootloader (`boot/boot.asm`)](#6-deep-dive-the-bootloader-bootbootasm)
7. [Deep dive: entering C (`kernel_entry.asm`, `linker.ld`)](#7-deep-dive-entering-c)
8. [Deep dive: the C kernel (`kernel.c`, `screen.c`)](#8-deep-dive-the-c-kernel)
9. [Deep dive: interrupts and the keyboard](#9-deep-dive-interrupts-and-the-keyboard)
10. [Deep dive: the shell](#10-deep-dive-the-shell)
11. [How the build system works](#11-how-the-build-system-works)
12. [Debugging when something goes wrong](#12-debugging-when-something-goes-wrong)
13. [Where to go from here (roadmap to a "real" OS)](#13-where-to-go-from-here)
14. [Glossary](#14-glossary)

---

## 1. What actually IS an operating system?

Strip away the marketing, and an OS is just **a program that runs first and never exits**, whose job is to manage hardware and run *other* programs on top of it. That's it. Windows, Linux, macOS — all gigantic versions of the same basic idea you're about to build:

1. Something has to run the very first instant the computer powers on.
2. That something has to take control of the screen, keyboard, disk, memory.
3. It has to provide *some* way for a human to interact with it (a shell, in our case).

Every other feature — files, processes, networking, graphics, multiple users — is built **on top of** those three things, incrementally, over decades. DeadIV OS gives you a real (if tiny) version of all three, and this README explains how to keep building on it.

---

## 2. What you need installed

This project targets a Debian/Ubuntu Linux machine. Install the toolchain with:

```bash
sudo apt-get update
sudo apt-get install -y nasm gcc-multilib qemu-system-x86
```

| Tool | What it's for |
|---|---|
| `nasm` | Assembler — turns `.asm` files into raw machine code |
| `gcc` (with `-m32` / multilib support) | Compiles our C kernel code into 32-bit x86 machine code |
| `ld` (comes with `binutils`, already on your system) | The **linker** — glues compiled files together into one binary |
| `objcopy` (comes with `binutils`) | Strips ELF metadata, leaving pure raw machine code |
| `qemu-system-x86` | A full virtual PC, so you can boot and test your OS without touching real hardware |

---

## 3. Quick start

```bash
cd DeadIV-OS
make          # builds build/os-image.bin
make run      # boots it in QEMU (a window will open)
```

Once it boots you'll see a banner and a `deadiv>` prompt. Try typing `help`, `about`, `echo hello world`, or `clear`.

To wipe all build output and start fresh:

```bash
make clean
```

---

## 4. The boot process, from power button to shell

This is the single most important mental model in this whole project. Read it twice.

```
 [Power button pressed]
         |
         v
 [CPU resets, jumps to a fixed address burned into the motherboard ROM]
         |
         v
 [BIOS runs: tests hardware (POST), finds a bootable disk]
         |
         v
 [BIOS loads the disk's FIRST 512 BYTES into RAM at address 0x7C00,
  and jumps there — IF the last 2 bytes are the signature 0x55 0xAA]
         |
         v
 +-------------------------------------------------------+
 |  boot/boot.asm  (our "boot sector", exactly 512 bytes) |
 |  Runs in 16-bit REAL MODE (1978-era CPU compatibility) |
 |    1. Sets up a small stack                            |
 |    2. Prints "16-bit real mode OK"                      |
 |    3. Asks the BIOS to read the KERNEL off disk into    |
 |       RAM at address 0x1000 (since we only have 512     |
 |       bytes here — nowhere near enough for a real OS)   |
 |    4. Builds a GDT (memory map) and flips the CPU into  |
 |       32-bit PROTECTED MODE                             |
 |    5. Jumps to address 0x1000 — into the kernel         |
 +-------------------------------------------------------+
         |
         v
 +-------------------------------------------------------+
 |  kernel/kernel_entry.asm  (first bytes of the kernel)  |
 |  Runs in 32-bit PROTECTED MODE                          |
 |    Calls kmain() — jumping from assembly into C          |
 +-------------------------------------------------------+
         |
         v
 +-------------------------------------------------------+
 |  kernel/kernel.c :: kmain()                             |
 |    1. screen_clear()                                    |
 |    2. idt_install()   — sets up interrupt handling       |
 |    3. shell_init()    — prints banner + first prompt      |
 |    4. for(;;) hlt;    — idle forever, waiting for          |
 |                          hardware interrupts (keypresses)  |
 +-------------------------------------------------------+
         |
         v
   [You type on the keyboard]
         |
         v
   Hardware interrupt fires -> idt_load.asm's isr_keyboard
   -> keyboard.c reads the key -> shell.c updates the prompt
   -> you see it appear on screen
```

Everything below is a detailed explanation of every box in that diagram.

---

## 5. Project file map

```
DeadIV-OS/
├── boot/
│   └── boot.asm          The boot sector: BIOS → real mode → protected mode
├── kernel/
│   ├── kernel_entry.asm  Tiny asm stub: first thing that runs in the kernel
│   ├── idt_load.asm      Asm glue for interrupts (LIDT + the keyboard ISR)
│   ├── kernel.c           kmain() — the "real" entry point, in C
│   ├── screen.c/.h        VGA text-mode screen driver (printing, colors, scroll)
│   ├── idt.c/.h            Interrupt Descriptor Table + PIC setup
│   ├── keyboard.c/.h       PS/2 keyboard driver (scancode -> ASCII)
│   ├── shell.c/.h          The interactive command shell
│   ├── string.c/.h         Our own strlen/strcmp/memset/memcpy (no libc!)
│   ├── ports.h              inb()/outb() — raw hardware I/O port access
│   └── types.h              Our own u8/u16/u32 types (no libc's stdint.h!)
├── linker.ld               Tells the linker how to lay out the kernel in memory
├── Makefile                 One command (`make`) builds everything
└── README.md                 This file
```

---

## 6. Deep dive: the bootloader (`boot/boot.asm`)

Open `boot/boot.asm` side-by-side with this section — every line in that file has a comment, but here's the big picture of *why* each piece exists.

### Why does it have to be assembly?

Because the CPU, the instant it powers on, understands **only raw machine instructions** — there's no C runtime, no `main()`, nothing. Assembly is the closest human-readable representation of those raw instructions. We keep this file as small and mechanical as possible specifically so we can escape into C as fast as possible (see `kernel_entry.asm`).

### Why 512 bytes? Why the magic number `0xAA55`?

This is a historical BIOS convention from the original IBM PC (1981) that every BIOS since has kept for backward compatibility. The BIOS has no idea what a "file system" or "operating system" is — it just blindly reads the first sector (512 bytes) of the boot disk into RAM at address `0x7C00`, checks that the last two bytes are `0x55, 0xAA`, and if so, jumps there. If those two bytes aren't right, the BIOS assumes the disk isn't bootable and tries the next device.

### Why "real mode"? Why can't we just start in normal 32-bit/64-bit mode?

Backward compatibility, again. The very first x86 CPU (the 8086, from 1978) only understood 16-bit instructions and could only address 1 MB of memory using a segmented addressing scheme. Every x86 CPU made since — including the one in your laptop right now — **still boots up pretending to be that 1978 chip**, for compatibility with 45 years of old software. It's our job, in the bootloader, to manually switch the CPU into a more modern mode. That's most of what `boot.asm` does.

### The three modes, summarized

| Mode | Introduced | Address space | Used for |
|---|---|---|---|
| Real mode | 1978 (8086) | 1 MB | The state every x86 CPU boots into |
| Protected mode | 1982 (80286), extended 1985 (80386) | 4 GB | What DeadIV OS runs in |
| Long mode | 2003 (AMD64) | Much more (64-bit) | What real modern OSes (Linux, Windows) run in — but you MUST pass through protected mode to get there |

We stop at protected mode because it's dramatically simpler to explain and still gives you flat 32-bit memory addressing — everything you need to understand the *concepts* transfers directly if you later extend this to 64-bit long mode.

### Step-by-step through what `boot.asm` actually does

1. **Zero the segment registers, set up a stack.** Real mode addresses memory as `segment * 16 + offset`. We zero everything so plain addresses mean what they look like they mean.
2. **Print a message using the BIOS.** We haven't written our own screen driver yet (that comes later in C) — so in this early stage, we borrow the BIOS's own built-in "print a character" service (`INT 0x10`).
3. **Load the kernel off disk using the BIOS.** Our kernel is far bigger than the 512 bytes we have to work with, so we ask the BIOS (`INT 0x13`, "read sectors") to copy it from disk into RAM at address `0x1000`, where we'll jump to it once we're in 32-bit mode.
4. **Build a GDT (Global Descriptor Table).** This is a small table describing memory "segments" — but we deliberately define a *boring* one: one segment for code, one for data, both spanning the **entire** 4 GB address space starting at 0. This is called the "flat memory model," and it means we can basically ignore segmentation from here on and just use plain 32-bit addresses like any modern program would.
5. **Flip one bit in the `CR0` control register.** This is the literal instant the CPU becomes a 32-bit protected-mode CPU. One bit.
6. **Do a "far jump."** This reloads the CPU's code segment register with our new GDT selector and flushes any leftover 16-bit instructions the CPU might have pre-fetched. Without this, the CPU could start misinterpreting bytes.
7. **Jump to address `0x1000`.** This hands control to the kernel we loaded in step 3, and the bootloader's job is permanently finished.

---

## 7. Deep dive: entering C

### `kernel/kernel_entry.asm`

The bootloader does `jmp 0x1000`. Whatever bytes sit at the very start of our compiled kernel binary are the first thing that runs. `kernel_entry.asm` is that: a two-instruction file whose only job is `call kmain` — jumping straight from assembly into our C function `kmain()` in `kernel.c`. From that point on, you will basically never touch assembly again.

### `linker.ld`

You might wonder: how does the compiler/linker know our kernel will be loaded at address `0x1000`? We tell it, explicitly, in `linker.ld`:

```
. = 0x1000;
```

This means: "pretend the output binary starts at memory address `0x1000`." Every function call, every global variable reference the C compiler generates gets computed relative to this base address, so that when the bootloader really does load us at `0x1000`, every address the compiler baked in is correct.

We also explicitly list `kernel_entry.o` **first** on the linker command line (see the Makefile), which guarantees its `call kmain` instruction ends up as the very first bytes of the final binary — i.e., exactly at address `0x1000`, exactly where the bootloader jumps.

### Why no standard library (`printf`, `malloc`, etc.)?

The C standard library assumes an operating system sits underneath it: `malloc()` needs virtual memory management, `printf("%d")` needs somewhere to *write* text (a terminal, a file — provided by an OS). **We are writing the OS.** There's nothing underneath us to provide those services. This is called writing "freestanding" C, and it's why you'll see:

- `types.h` — our own `u8`/`u16`/`u32` instead of `<stdint.h>`
- `string.h`/`string.c` — our own `k_strlen`/`k_memcpy` instead of `<string.h>`
- `screen.c`'s `print_string`/`print_int` instead of `printf`

Every single one of these is a tiny, readable reimplementation — read them, they're short.

---

## 8. Deep dive: the C kernel

### `kernel.c` — `kmain()`

This is the "true" main function. It runs subsystem setup in a specific, important order:

```c
screen_clear();     // 1. Get a clean screen
idt_install();      // 2. Set up interrupt handling BEFORE anything can interrupt us
shell_init();        // 3. Print the banner + first prompt
for (;;) { hlt; }    // 4. Idle forever
```

Order matters here: if we waited to install the IDT until *after* enabling something that could fire an interrupt, an interrupt could arrive with no handler table set up, and the CPU would crash (a "triple fault" — see the debugging section).

The final `hlt` loop is important to understand: DeadIV OS has no other work to do between key presses, so it politely tells the CPU "stop executing instructions until the next hardware interrupt arrives." This uses essentially 0% CPU while idle, compared to a `while(1);` empty loop which would burn 100% of a CPU core doing nothing.

### `screen.c` — how does printing text even work with no drivers?

This is one of the most magical-feeling parts of OS dev the first time you see it, so it's worth understanding deeply.

VGA hardware, in its default "text mode," exposes the entire 80×25 character screen as **ordinary memory** at the fixed physical address `0xB8000`. This is called **memory-mapped I/O**: instead of talking to a device over a special I/O port, you just read/write normal memory, and the hardware watches that address range and updates the physical display whenever you write to it.

Each on-screen character cell is 2 bytes:

```
byte 0: the ASCII character
byte 1: the "attribute byte" — low nibble = foreground color, high nibble = background color
```

So `screen.c`'s entire job is arithmetic: given a cursor row/column, compute `0xB8000 + (row * 80 + col) * 2`, and write the character + color byte there. `print_string()` just calls this in a loop. `screen_clear()` writes a blank space to every cell. `scroll_if_needed()` copies each row up by one when the cursor runs off the bottom — literally implementing scrolling by hand, with a for-loop over memory.

There's no such thing as a "screen API call" here. It's just memory.

---

## 9. Deep dive: interrupts and the keyboard

### What is an interrupt?

Hardware devices don't patiently wait for the CPU to poll them. When something happens — a key is pressed! — the device electrically signals the CPU: "stop what you're doing and come deal with me, right now." This signal is a **hardware interrupt**. The CPU needs to know, for every possible interrupt number, exactly which function to run. That table is the **IDT** (Interrupt Descriptor Table) — see `idt.c`/`idt.h`.

### The PIC and "remapping" — a classic OS-dev gotcha

There's an old chip called the **PIC** (Programmable Interrupt Controller) that takes raw hardware signals (called "IRQs," numbered 0-15: IRQ0 is the system timer, IRQ1 is the keyboard, etc.) and turns them into CPU interrupts. **By default, on boot, the PIC uses CPU interrupt numbers 0-15 for these** — which collides head-on with the CPU's own *reserved* interrupts 0-15 (things like "divide by zero" and "page fault"). Every real x86 OS must "remap" the PIC at startup to use a different range instead. `idt.c`'s `pic_remap()` function does exactly this, moving IRQs to interrupt numbers 32-47 by sending a specific byte sequence to two I/O ports the PIC listens on (`0x20`/`0x21` for the master chip, `0xA0`/`0xA1` for the slave chip). This is standard PC boilerplate — every hobby OS on earth has a nearly identical function.

### Why does `isr_keyboard` have to be written in assembly?

Hardware interrupts don't follow C's normal "function call" convention — the CPU doesn't push a return address the way a `call` instruction does, and returning from an interrupt requires the special `iret` instruction (which also restores the CPU flags register), not a normal `ret`. So `idt_load.asm`'s `isr_keyboard` is a small **trampoline**:

1. `pushad` — save every single CPU register, because the code that got interrupted has no idea it was interrupted, and expects every register to be untouched when it resumes.
2. `call keyboard_handler_main` — jump into normal, readable C code to do the real work.
3. Send the PIC an "End Of Interrupt" signal (`out 0x20, 0x20`) — **critical**: without this, the PIC assumes we're still busy handling the last interrupt and will never send us another one. Forget this line and your keyboard appears to work exactly once, then dies forever.
4. `popad` — restore every register.
5. `iret` — properly resume whatever was running before the key was pressed.

### `keyboard.c` — scancodes are NOT ASCII

When you press a key, the keyboard doesn't send `'A'`. It sends a **scancode** — a raw number identifying a physical key *position* on the keyboard, standardized decades ago as "Scancode Set 1." We read the scancode from I/O port `0x60` and translate it to ASCII using a hand-written lookup table (`scancode_to_ascii[]`). Two important details:

- If the scancode's high bit is set (`scancode & 0x80`), it means the key was **released**, not pressed. We ignore releases in this simple driver.
- Special keys (arrows, function keys) send a 2-byte sequence starting with `0xE0`, which our simple table doesn't yet decode — see the roadmap section for how you'd add that.

---

## 10. Deep dive: the shell

`shell.c` is deliberately simple: it's not a real program loader, just a fixed set of built-in commands compiled directly into the kernel (exactly how the very earliest real OS shells worked). Its entire logic is **event-driven**, with no polling loop of its own:

- `shell_handle_char(c)` is called once, directly, every time `keyboard.c` translates a key press to ASCII.
- Regular characters get appended to `input_buffer` and echoed to the screen.
- `'\b'` (backspace) removes the last character, both from the buffer and visually from the screen.
- `'\n'` (Enter) null-terminates the buffer, hands it to `run_command()`, and resets for the next line.
- `run_command()` is a chain of `k_strcmp` comparisons against known command names (`help`, `about`, `clear`, `echo ...`).

Adding a new command is as simple as adding another `else if (k_strcmp(cmd, "yourcommand") == 0) { ... }` block.

---

## 11. How the build system works

Running `make` triggers this chain (see `Makefile` for the exact commands, each one commented):

1. **Assemble** `boot.asm` → `boot.bin`, a raw 512-byte flat binary (no file format headers — the BIOS wants pure machine code).
2. **Assemble** `kernel_entry.asm` and `idt_load.asm` → ELF object files (`.o`), since these need to be linked together with our C code.
3. **Compile** every `.c` file → `.o` object files, using flags that disable every "assume there's an OS underneath me" behavior GCC has (`-ffreestanding -nostdlib -fno-pie -fno-stack-protector -fno-builtin`).
4. **Link** all the object files together, using `linker.ld` to control memory layout, into one `kernel.elf`.
5. **Strip** the ELF metadata with `objcopy`, leaving `kernel.bin` — pure machine code, exactly what needs to sit in memory at `0x1000`.
6. **Concatenate** `boot.bin` + `kernel.bin` → `os-image.bin`, our final bootable disk image, then **pad it** to a standard 1.44 MB floppy size (this matters — see the comment in the Makefile about why an unpadded image can cause the BIOS's disk-read call to fail).

`make run` boots this image directly in QEMU: `qemu-system-i386 -fda build/os-image.bin`.

---

## 12. Debugging when something goes wrong

OS development bugs are uniquely brutal because there's no OS underneath you to catch your mistakes and print a friendly error — a bad pointer can crash the *entire virtual machine* instantly with zero explanation. Here's your toolkit:

**Triple faults (instant reboot / blank screen):** This is what happens when an interrupt fires and the CPU can't even find a valid handler for it (or the handler itself crashes). Run:

```bash
qemu-system-i386 -fda build/os-image.bin -d int,cpu_reset -D qemu.log -no-reboot
```

Then read `qemu.log` — it logs every interrupt taken, which is often enough to spot exactly where things went wrong.

**GDB source-level debugging:** QEMU can pause at boot and wait for a debugger:

```bash
make run-debug     # starts QEMU paused, listening on port 1234
```

In another terminal:

```bash
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kmain
(gdb) continue
```

You can now step through your actual C code line-by-line, exactly like debugging a normal program.

**"It just hangs on 'Loading kernel from disk...'":** Almost always means `KERNEL_SECTORS` in `boot.asm` is asking the BIOS to read more sectors than actually exist in your disk image file. Rebuild with `make clean && make` (the padding step in the Makefile handles this), or increase the padded size if your kernel grows very large.

**Screen shows garbage / wrong colors:** Double check you're writing `(char, attribute)` pairs in the right order to VGA memory, and that you're not accidentally writing past column 79 or row 24 without wrapping.

---

## 13. Where to go from here

DeadIV OS is a genuine, working foundation — bootloader, protected mode, interrupts, a driver, a shell — but it is intentionally a *starting point*. Real operating systems add, roughly in this order of typical difficulty:

1. **A timer driver (IRQ0 / PIT chip).** Nearly identical structure to the keyboard driver — this unlocks the concept of "ticks" and is the first step toward multitasking.
2. **Paging / virtual memory.** Lets each program believe it has its own private memory space, and is the foundation of memory protection (crashing one program can't crash the whole OS).
3. **A simple heap allocator (`kmalloc`).** Right now we have no dynamic memory allocation at all — every buffer in this OS is a fixed-size global array.
4. **Multitasking / a scheduler.** Uses the timer interrupt to save one task's registers and load another's, giving the illusion of many programs running "simultaneously."
5. **User mode (ring 3).** Right now, everything — including the shell — runs with full, unrestricted hardware access ("ring 0" / kernel mode). Real OSes run user programs in a restricted mode and only the kernel itself runs unrestricted.
6. **A real filesystem driver**, so programs can be loaded from disk rather than compiled directly into the kernel like our shell commands are.
7. **64-bit long mode.** Once protected mode and paging make sense to you, the jump to 64-bit is a well-documented, incremental extension of exactly the same ideas in this README.

Excellent next resources once you've internalized this project: the **OSDev Wiki** (osdev.org) and the **Intel/AMD Software Developer Manuals** (the actual, authoritative specification for everything in this README — dry, but 100% authoritative).

---

## 14. Glossary

| Term | Meaning |
|---|---|
| **BIOS** | Firmware built into the motherboard; runs first, tests hardware, loads the boot sector |
| **Boot sector** | The first 512 bytes of a bootable disk, ending in the signature `0x55 0xAA` |
| **Real mode** | The 16-bit, 1 MB-limited compatibility mode every x86 CPU starts in |
| **Protected mode** | The 32-bit, 4 GB mode real (simple) OSes run in |
| **GDT** | Global Descriptor Table — describes memory "segments" available to the CPU |
| **IDT** | Interrupt Descriptor Table — maps interrupt numbers to handler function addresses |
| **PIC** | Programmable Interrupt Controller — routes hardware IRQs to CPU interrupts |
| **IRQ** | Interrupt Request — a hardware device's raw "please service me" signal line |
| **ISR** | Interrupt Service Routine — the function that runs when an interrupt fires |
| **Scancode** | A raw number identifying a physical keyboard key, NOT an ASCII character |
| **Freestanding** | C code with no standard library / no assumed OS underneath it |
| **Linker script** | Instructions telling the linker exactly how to lay out a binary in memory |
| **Memory-mapped I/O** | A device that you talk to via ordinary memory reads/writes instead of I/O ports |
| **I/O port** | A separate, 65536-slot address space on x86 used by `in`/`out` instructions for older hardware |
| **Kernel** | The core of an OS — the part that manages hardware and runs first |
| **Ring 0 / kernel mode** | The unrestricted CPU privilege level everything in DeadIV OS currently runs at |

---

Built by working through boot sectors, protected mode, interrupts, and drivers one file at a time — every line commented so you can read the *source code itself* as the real tutorial. Good luck, and welcome to OS development.
