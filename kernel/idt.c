/* =============================================================================
 * idt.c — Interrupt Descriptor Table + PIC remapping
 * Installs 256 IDT entries (32 CPU exceptions + 16 IRQs + remaining unused).
 * Remaps PIC so IRQ0-15 map to vectors 32-47, avoiding conflict with CPU
 * exceptions (0-31).
 * ============================================================================= */

#include "../include/idt.h"
#include "../include/io.h"
#include "../include/vga.h"

/* ── PIC I/O ports ───────────────────────────────────────────────────────── */
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20     /* End-Of-Interrupt command */

/* ── IDT storage ─────────────────────────────────────────────────────────── */
static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* ── User-registered ISR callbacks ──────────────────────────────────────── */
static isr_handler_t isr_handlers[256];

/* ── Assembly: load IDTR ─────────────────────────────────────────────────── */
extern void idt_flush(uint32_t idt_ptr_addr);

/* ── All 256 ISR stubs declared in isr_stubs.asm ────────────────────────── */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* IRQ stubs (vectors 32-47) */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* ── Helper: fill one IDT entry ─────────────────────────────────────────── */
static void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = (uint16_t)(base & 0xFFFF);
    idt[n].base_high = (uint16_t)(base >> 16);
    idt[n].selector  = sel;
    idt[n].zero      = 0;
    idt[n].flags     = flags;   /* e.g. 0x8E = P|DPL0|32-bit interrupt gate */
}

/* ── PIC initialisation and remapping ───────────────────────────────────── */
static void pic_remap(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: start initialisation cascade, expect ICW4 */
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    /* ICW2: vector offsets */
    outb(PIC1_DATA, 0x20); io_wait();   /* IRQ 0-7  → vectors 32-39 */
    outb(PIC2_DATA, 0x28); io_wait();   /* IRQ 8-15 → vectors 40-47 */

    /* ICW3: cascade identity */
    outb(PIC1_DATA, 0x04); io_wait();   /* master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();   /* slave: cascade identity = 2 */

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void idt_register_handler(uint8_t n, isr_handler_t handler) {
    isr_handlers[n] = handler;
}

/* Called by every ISR/IRQ stub (defined in isr_stubs.asm) */
void isr_common_handler(registers_t *regs) {
    if (isr_handlers[regs->int_no]) {
        isr_handlers[regs->int_no](regs);
    } else if (regs->int_no < 32) {
        /* Unhandled CPU exception — print and halt */
        vga_set_color(COLOR_WHITE, COLOR_RED);
        vga_puts("\n*** KERNEL PANIC: Unhandled exception #");
        vga_put_dec(regs->int_no);
        vga_puts("  err=");
        vga_put_hex(regs->err_code);
        vga_puts("  eip=");
        vga_put_hex(regs->eip);
        vga_puts(" ***\n");
        for (;;) { __asm__ volatile("hlt"); }
    }
    /* Send EOI for hardware IRQs */
    if (regs->int_no >= 32 && regs->int_no < 48) {
        if (regs->int_no >= 40) outb(PIC2_CMD, PIC_EOI);
        outb(PIC1_CMD, PIC_EOI);
    }
}

void idt_init(void) {
    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint32_t)&idt;

    /* Zero the whole table first */
    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0x08, 0x8E);
        isr_handlers[i] = 0;
    }

    /* CPU exception stubs (vectors 0-31) */
    idt_set_gate( 0, (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate( 1, (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate( 2, (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate( 3, (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate( 4, (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate( 5, (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate( 6, (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate( 7, (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate( 8, (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate( 9, (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* Remap PIC then install IRQ stubs (vectors 32-47) */
    pic_remap();
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    idt_flush((uint32_t)&idt_ptr);
}
