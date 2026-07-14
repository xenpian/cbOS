#ifndef GDT_H
#define GDT_H

#include "types.h"

/* GDT segment selectors (must match entries in gdt.c) */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20

/* One GDT entry (8 bytes) */
typedef struct {
    uint16_t limit_low;     /* Lower 16 bits of segment limit        */
    uint16_t base_low;      /* Lower 16 bits of base address         */
    uint8_t  base_mid;      /* Bits 16-23 of base address            */
    uint8_t  access;        /* Access byte: P | DPL | S | Type       */
    uint8_t  gran;          /* Granularity: G | D/B | L | AVL | lim  */
    uint8_t  base_high;     /* Bits 24-31 of base address            */
} PACKED gdt_entry_t;

/* GDT pointer loaded with lgdt */
typedef struct {
    uint16_t limit;         /* Size of GDT - 1                       */
    uint32_t base;          /* Linear address of GDT                 */
} PACKED gdt_ptr_t;

void gdt_init(void);

#endif /* GDT_H */
