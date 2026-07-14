/* =============================================================================
 * gdt.c — Global Descriptor Table
 * Sets up 5 descriptors: null, kernel code, kernel data, user code, user data.
 * The bootloader already loaded a minimal GDT; we replace it here with a
 * proper one from C so the rest of the kernel can rely on known selectors.
 * ============================================================================= */

#include "../include/gdt.h"

/* ── GDT storage ─────────────────────────────────────────────────────────── */
#define GDT_ENTRIES 5
static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* ── Assembly helper: flush segment registers after lgdt ─────────────────── */
extern void gdt_flush(uint32_t gdt_ptr_addr);  /* gdt_flush.asm */

/* ── Build one GDT entry ─────────────────────────────────────────────────── */
static void gdt_set_entry(int idx,
                          uint32_t base,
                          uint32_t limit,
                          uint8_t  access,
                          uint8_t  gran)
{
    gdt[idx].base_low  = (uint16_t)(base  & 0xFFFF);
    gdt[idx].base_mid  = (uint8_t)((base  >> 16) & 0xFF);
    gdt[idx].base_high = (uint8_t)((base  >> 24) & 0xFF);

    gdt[idx].limit_low = (uint16_t)(limit & 0xFFFF);
    /* Upper nibble of gran holds bits 16-19 of limit */
    gdt[idx].gran      = (uint8_t)((gran & 0xF0) | ((limit >> 16) & 0x0F));
    gdt[idx].access    = access;
}

/* ── Public init ─────────────────────────────────────────────────────────── */
void gdt_init(void) {
    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)&gdt;

    /* 0: Null descriptor (required by x86 spec) */
    gdt_set_entry(0, 0, 0, 0x00, 0x00);

    /* 1: Kernel code — base 0, limit 4 GB, ring 0, execute/read
     *    Access : 0x9A = 1001 1010b  (P=1, DPL=00, S=1, Type=1010)
     *    Gran   : 0xCF = 1100 1111b  (G=1 4KB, D/B=1 32-bit, limit hi=F) */
    gdt_set_entry(1, 0x00000000, 0xFFFFFFFF, 0x9A, 0xCF);

    /* 2: Kernel data — base 0, limit 4 GB, ring 0, read/write
     *    Access : 0x92 = 1001 0010b */
    gdt_set_entry(2, 0x00000000, 0xFFFFFFFF, 0x92, 0xCF);

    /* 3: User code — same as kernel code but DPL=3
     *    Access : 0xFA = 1111 1010b */
    gdt_set_entry(3, 0x00000000, 0xFFFFFFFF, 0xFA, 0xCF);

    /* 4: User data — same as kernel data but DPL=3
     *    Access : 0xF2 = 1111 0010b */
    gdt_set_entry(4, 0x00000000, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Load GDTR and flush segment registers */
    gdt_flush((uint32_t)&gdt_ptr);
}
