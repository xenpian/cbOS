# cbOS

A tiny x86 operating system kernel written from scratch in C and x86 assembly.
Boots on real hardware and QEMU. No external libraries — zero dependency on libc or any OS runtime.

```
  __  __ _       _    ___  ____
 |  \/  (_)_ __ (_)  / _ \/ ___|
 | |\/| | | '_ \| | | | | \___ \
 | |  | | | | | | | | |_| |___) |
 |_|  |_|_|_| |_|_|  \___/|____/
```

---

## Architecture overview

```
cbOS/
├── boot/
│   └── boot.asm          16-bit MBR bootloader (BIOS INT 13h, A20, GDT, PE)
├── kernel/
│   ├── kernel_entry.asm  32-bit entry point: segments, BSS zero, stack setup
│   ├── kernel.c          kernel_main(): subsystem init sequence
│   ├── gdt.c             Global Descriptor Table (5 descriptors)
│   ├── gdt_flush.asm     lgdt + far-jump + segment reload
│   ├── idt.c             Interrupt Descriptor Table (256 gates) + PIC remap
│   ├── isr_stubs.asm     All 32 CPU exception stubs + 16 IRQ stubs
│   ├── shell.c           Interactive shell (8 built-in commands)
│   └── string.c          Freestanding string / memory helpers
├── drivers/
│   ├── vga.c             VGA text-mode 80×25, 16 colours, HW cursor
│   └── keyboard.c        PS/2 scan-code set 1, ring buffer, readline
├── mm/
│   └── mm.c              Bitmap page allocator + free-list heap + paging
├── include/              Header files for every module
├── kernel.ld             Linker script (flat binary, load at 0x1000)
└── Makefile              Build + QEMU targets
```

---

## What it does

| Subsystem | Details |
|-----------|---------|
| **Bootloader** | 512-byte MBR. Reads 64 sectors (32 KB) from LBA 1 into 0x1000 via INT 13h, enables A20, sets up a minimal GDT, switches to 32-bit protected mode, jumps to the kernel. |
| **GDT** | 5 descriptors: null, kernel code (0x08), kernel data (0x10), user code (0x18), user data (0x20). Loaded with `lgdt` + far jump to flush the pipeline. |
| **IDT** | 256 gates. PIC 8259A remapped so IRQ 0-15 → vectors 32-47 (avoiding collision with CPU exceptions 0-31). `isr_common_handler` dispatches to registered C callbacks or kernel-panics. |
| **VGA** | Direct write to 0xB8000. 80×25 colour text mode, hardware cursor via CRTC registers (0x3D4/0x3D5). Handles `\n \r \t \b`. Scrolls on overflow. |
| **Keyboard** | PS/2 IRQ1 handler. Translates scan-code set 1 to ASCII. US QWERTY layout, shift + caps-lock support. 256-byte ring buffer. Blocking `getchar` sleeps with `hlt`. |
| **Memory** | Physical: bitmap over a 4 MB heap at 0x200000 (1024 × 4 KB pages). Heap: block-header linked list, first-fit, split on alloc, coalesce on free. Paging: identity maps 0–8 MB, enables CR0.PG. |
| **Shell** | `help` `clear` `echo` `mem` `hex` `about` `reboot` `halt` |

---

## Prerequisites

You need a **cross-compiler** targeting bare-metal i686 (no OS ABI).
The easiest path is `i686-elf-gcc` from [OSDev.org's cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler).

### Quick install (most Linux distros)

```bash
# Assembler + emulator
sudo apt install nasm qemu-system-x86

# Cross-compiler (pre-built packages — Ubuntu/Debian)
sudo apt install gcc-multilib      # only if you want to build the toolchain yourself

# Or grab pre-built binaries from:
# https://github.com/nativeos/i686-elf-toolchain/releases
# then add them to PATH
```

### macOS (Homebrew)

```bash
brew install nasm qemu
brew tap nativeos/i686-elf-toolchain
brew install i686-elf-binutils i686-elf-gcc
```

### Windows

Use **WSL2** (Ubuntu) and follow the Linux instructions above.

---

## Build

```bash
cd cbOS
make
```

Produces **`os.img`** — a 1.44 MB floppy image containing the bootloader + kernel.

```
  os.img built successfully
  Bootloader : 512 bytes
  Kernel     : ~18 KB
```

---

## Run

### QEMU — VGA window (recommended)

```bash
make run-gui
```

### QEMU — headless / serial

```bash
make run
```

### Real hardware

Write the image to a USB drive (***this will erase the drive***):

```bash
# Linux
sudo dd if=os.img of=/dev/sdX bs=512 conv=fdatasync status=progress

# macOS
sudo dd if=os.img of=/dev/rdiskN bs=512
```

Boot the machine from that device.

---

## Shell commands

Once cbOS boots you will see a prompt:

```
cbOS:~$
```

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text |
| `mem` | Show heap usage (used / free / total) |
| `hex <n>` | Convert a decimal number to hex |
| `about` | Kernel build info |
| `reboot` | Triple-fault reboot |
| `halt` | `cli; hlt` — safe power-off |

---

## Memory map

```
0x00000000 – 0x000FFFFF   Real-mode IVT, BDA, BIOS ROM, VGA buffer
0x00100000 – 0x001FFFFF   Kernel image (≤ 1 MB)
0x00200000 – 0x005FFFFF   Kernel heap (4 MB, managed by mm.c)
0x00600000 – 0x007FFFFF   Remaining identity-mapped pages (free)
```

---

## How to extend it

| Goal | Where to start |
|------|---------------|
| Add a new shell command | `kernel/shell.c` → `dispatch()` function |
| Handle a new IRQ | `idt_register_handler(IRQ_N, my_handler)` |
| Add a new driver | Create `drivers/mydriver.c`, add header to `include/`, add to `Makefile` `C_SRCS` |
| Implement `printf` | Add `kernel/printf.c` using `vga_putchar` as the sink |
| User-mode processes | Add TSS to GDT, implement `iret` to ring-3 |
| File system | Implement FAT12 reading from floppy sectors |

---

## Troubleshooting

**`i686-elf-gcc: command not found`**
→ Install the cross-compiler and make sure it is on your `PATH`.

**QEMU shows "Boot failed: not a bootable disk"**
→ Run `make clean && make` to rebuild from scratch.

**Kernel triple-faults on startup**
→ The kernel binary may be larger than 32 KB (64 sectors × 512 bytes).
  Increase `KERNEL_SECTORS` in `boot/boot.asm` and match the padding in the `Makefile`.

**No keyboard input in QEMU**
→ Click inside the QEMU window to capture the mouse/keyboard, or pass `-display gtk` to `qemu-system-i386`.

---

## License

MIT — do whatever you want with it.
