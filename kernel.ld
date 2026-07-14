/* =============================================================================
 * kernel.ld — Linker script for cbOS kernel
 *
 * The bootloader loads the raw kernel binary to physical address 0x1000 and
 * jumps to it.  We therefore set the load address (LMA = VMA) to 0x1000 and
 * tell the linker to place kernel_entry.o first so the very first byte the
 * CPU executes is _start.
 * ============================================================================= */

OUTPUT_FORMAT("binary")       /* flat binary — no ELF header in the image     */
OUTPUT_ARCH(i386)

ENTRY(_start)

SECTIONS {

    /* ── Code starts at 0x1000 (where bootloader jumped) ─────────────────── */
    . = 0x1000;

    /* ── Text section ─────────────────────────────────────────────────────── */
    .text : {
        /* kernel_entry MUST be first: it contains _start */
        kernel/kernel_entry.o (.text)
        kernel/gdt_flush.o    (.text)
        kernel/isr_stubs.o    (.text)

        /* All remaining .text sections */
        *(.text*)
    }

    /* ── Read-only data ───────────────────────────────────────────────────── */
    .rodata : {
        *(.rodata*)
    }

    /* ── Initialised data ─────────────────────────────────────────────────── */
    .data : {
        *(.data*)
    }

    /* ── BSS (zero-initialised) ───────────────────────────────────────────── */
    /* kernel_entry.asm zeroes this region before calling kernel_main()       */
    .bss : {
        _bss_start = .;
        *(COMMON)
        *(.bss*)
        _bss_end = .;
    }

    /* Discard debug / comment sections to keep the binary small */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.debug*)
    }
}
