#ifndef IDT_H
#define IDT_H

#include "types.h"

/* One IDT entry (8 bytes) */
typedef struct {
    uint16_t base_low;      /* Lower 16 bits of handler address      */
    uint16_t selector;      /* Kernel code segment selector          */
    uint8_t  zero;          /* Always 0                              */
    uint8_t  flags;         /* P | DPL | 0 | Gate type               */
    uint16_t base_high;     /* Upper 16 bits of handler address      */
} PACKED idt_entry_t;

/* IDT pointer loaded with lidt */
typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idt_ptr_t;

/* CPU register state pushed by ISR stubs */
typedef struct {
    /* Pushed by pusha */
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    /* Pushed by ISR stub */
    uint32_t int_no;
    uint32_t err_code;
    /* Pushed automatically by CPU */
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

/* Register an interrupt handler callback */
typedef void (*isr_handler_t)(registers_t *regs);
void idt_register_handler(uint8_t n, isr_handler_t handler);

void idt_init(void);

/* IRQ re-mapping offset */
#define IRQ0  32
#define IRQ1  33
#define IRQ2  34
#define IRQ12 44

#endif /* IDT_H */
