/* =============================================================================
 * kernel.c — kernel_main(): initialise subsystems and start the shell
 * ============================================================================= */

#include "../include/types.h"
#include "../include/vga.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/keyboard.h"
#include "../include/mm.h"
#include "../include/shell.h"

/* ── Banner ──────────────────────────────────────────────────────────────── */
static void print_banner(void) {
    vga_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    vga_puts("  __  __ _       _    ___  ____  \n");
    vga_puts(" |  \\/  (_)_ __ (_)  / _ \\/ ___| \n");
    vga_puts(" | |\\/| | | '_ \\| | | | | \\___ \\ \n");
    vga_puts(" | |  | | | | | | | | |_| |___) |\n");
    vga_puts(" |_|  |_|_|_| |_|_|  \\___/|____/ \n");
    vga_set_color(COLOR_DARK_GREY, COLOR_BLACK);
    vga_puts(" A tiny x86 kernel written in C + ASM\n");
    vga_puts(" ======================================\n\n");
    vga_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
}

/* ── Boot log helper ─────────────────────────────────────────────────────── */
static void ok(const char *msg) {
    vga_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    vga_puts(" [ OK ] ");
    vga_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    vga_puts(msg);
    vga_putchar('\n');
}

/* ── Entry point (called from kernel_entry.asm) ──────────────────────────── */
void kernel_main(void) {
    /* 1. VGA first — everything else may print */
    vga_init();
    print_banner();

    /* 2. Global Descriptor Table */
    gdt_init();
    ok("GDT loaded (5 descriptors, ring 0/3)");

    /* 3. Interrupt Descriptor Table + PIC */
    idt_init();
    ok("IDT loaded (256 gates, PIC remapped IRQ→32)");

    /* 4. Physical memory + heap */
    mm_init();
    ok("Memory manager ready (4 MB heap @ 0x200000)");

    /* 5. Enable paging (identity map 0–8 MB) */
    paging_init();
    ok("Paging enabled (identity map 0–8 MB)");

    /* 6. PS/2 keyboard */
    keyboard_init();
    ok("PS/2 keyboard driver installed");

    /* 7. Enable interrupts */
    __asm__ volatile("sti");
    ok("Interrupts enabled");

    /* Memory info */
    vga_set_color(COLOR_YELLOW, COLOR_BLACK);
    vga_puts("\n Heap: ");
    vga_put_dec((uint32_t)(mm_free() / 1024));
    vga_puts(" KB free / ");
    vga_put_dec((uint32_t)(4096));
    vga_puts(" KB total\n\n");
    vga_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);

    /* 8. Hand off to the shell — never returns */
    shell_run();

    /* Should never be reached */
    for (;;) __asm__ volatile("hlt");
}
