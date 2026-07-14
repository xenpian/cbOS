# =============================================================================
# Makefile — cbOS build system
#
# Prerequisites (install with your distro's package manager):
#   nasm            — assembler
#   i686-elf-gcc    — freestanding cross-compiler (no libc)
#   i686-elf-ld     — cross-linker
#   qemu-system-i386 — emulator for testing
#
# Targets:
#   make            — build os.img
#   make run        — build + launch in QEMU (no GUI, serial to stdio)
#   make run-gui    — build + launch in QEMU with VGA window
#   make clean      — remove all build artefacts
# =============================================================================

# ── Tools ─────────────────────────────────────────────────────────────────────
AS       := nasm
CC       := i686-elf-gcc
LD       := i686-elf-ld
QEMU     := qemu-system-i386

# ── Flags ─────────────────────────────────────────────────────────────────────
ASFLAGS  := -f elf32
CFLAGS   := -m32 \
             -std=c99 \
             -ffreestanding \
             -fno-builtin \
             -fno-stack-protector \
             -fno-pic \
             -Wall \
             -Wextra \
             -O2 \
             -I./include
LDFLAGS  := -T kernel.ld \
             -m elf_i386 \
             --oformat binary

QEMUFLAGS := -drive format=raw,file=os.img \
              -m 32M

# ── Source files ──────────────────────────────────────────────────────────────

# Bootloader (16-bit, raw binary — assembled separately)
BOOT_SRC := boot/boot.asm
BOOT_BIN := build/boot.bin

# Assembly objects (32-bit ELF, linked with the kernel)
ASM_SRCS := kernel/kernel_entry.asm \
             kernel/gdt_flush.asm   \
             kernel/isr_stubs.asm

# C sources
C_SRCS   := kernel/kernel.c   \
             kernel/gdt.c      \
             kernel/idt.c      \
             kernel/string.c   \
             kernel/shell.c    \
             drivers/vga.c     \
             drivers/keyboard.c \
             mm/mm.c

# Derived object paths (all land in build/)
ASM_OBJS := $(patsubst %.asm, build/%.o, $(ASM_SRCS))
C_OBJS   := $(patsubst %.c,   build/%.o, $(C_SRCS))
ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

KERNEL_BIN := build/kernel.bin

# ── Default target ─────────────────────────────────────────────────────────────
.PHONY: all run run-gui clean

all: os.img

# ── Create build directory tree ────────────────────────────────────────────────
build/boot build/kernel build/drivers build/mm:
	mkdir -p $@

# ── Bootloader ─────────────────────────────────────────────────────────────────
$(BOOT_BIN): $(BOOT_SRC) | build/boot
	$(AS) -f bin -o $@ $<

# ── Assembly kernel objects ────────────────────────────────────────────────────
build/kernel/%.o: kernel/%.asm | build/kernel
	$(AS) $(ASFLAGS) -o $@ $<

# ── C kernel objects ───────────────────────────────────────────────────────────
build/kernel/%.o: kernel/%.c | build/kernel
	$(CC) $(CFLAGS) -c -o $@ $<

build/drivers/%.o: drivers/%.c | build/drivers
	$(CC) $(CFLAGS) -c -o $@ $<

build/mm/%.o: mm/%.c | build/mm
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Link kernel to flat binary ─────────────────────────────────────────────────
$(KERNEL_BIN): $(ALL_OBJS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# ── Combine bootloader + kernel into a single disk image ──────────────────────
# The image must be padded to a whole number of 512-byte sectors.
os.img: $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > os.img.tmp
	@# Pad to 1.44 MB floppy size (optional but tidy)
	dd if=/dev/zero bs=512 count=2880 of=os.img status=none
	dd if=os.img.tmp of=os.img conv=notrunc status=none
	rm -f os.img.tmp
	@echo ""
	@echo "  os.img built successfully"
	@echo "  Bootloader : $(shell wc -c < $(BOOT_BIN)) bytes"
	@echo "  Kernel     : $(shell wc -c < $(KERNEL_BIN)) bytes"
	@echo ""

# ── Run in QEMU (no display window — serial output) ───────────────────────────
run: os.img
	$(QEMU) $(QEMUFLAGS) -nographic

# ── Run in QEMU with VGA window ───────────────────────────────────────────────
run-gui: os.img
	$(QEMU) $(QEMUFLAGS)

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf build os.img
