/* =============================================================================
 * mm.c — Kernel memory manager
 *
 * Two layers:
 *   1. Physical page allocator — bitmap over 4 MB heap, O(n) first-fit.
 *   2. Heap allocator (kmalloc/kfree) — block header linked list over the
 *      physical pages; simple first-fit with coalescing on free.
 *
 * Memory map (physical):
 *   0x000000 – 0x0FFFFF   First 1 MB (real-mode IVT, BDA, BIOS, VGA)
 *   0x100000 – 0x1FFFFF   Kernel image (~1 MB for comfort)
 *   0x200000 – 0x5FFFFF   Kernel heap (HEAP_START … HEAP_START+HEAP_SIZE)
 * ============================================================================= */

#include "../include/mm.h"
#include "../include/vga.h"

/* ── Physical page allocator ─────────────────────────────────────────────── */

#define PAGE_COUNT   (HEAP_SIZE / PAGE_SIZE)    /* 1024 pages × 4 KB = 4 MB */

/* 1 bit per page: 0 = free, 1 = used */
static uint32_t phys_bitmap[PAGE_COUNT / 32];

static void bitmap_set(uint32_t idx)   { phys_bitmap[idx / 32] |=  (1u << (idx % 32)); }
static void bitmap_clear(uint32_t idx) { phys_bitmap[idx / 32] &= ~(1u << (idx % 32)); }
static bool bitmap_test(uint32_t idx)  { return (phys_bitmap[idx / 32] >> (idx % 32)) & 1; }

/* Allocate n contiguous physical pages; returns base address or 0 on failure */
static uintptr_t phys_alloc_pages(uint32_t n) {
    uint32_t run = 0, start = 0;
    for (uint32_t i = 0; i < PAGE_COUNT; i++) {
        if (!bitmap_test(i)) {
            if (run == 0) start = i;
            if (++run == n) {
                for (uint32_t j = start; j < start + n; j++) bitmap_set(j);
                return HEAP_START + (uintptr_t)start * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0; /* OOM */
}

static void phys_free_pages(uintptr_t addr, uint32_t n) {
    if (addr < HEAP_START) return;
    uint32_t start = (uint32_t)((addr - HEAP_START) / PAGE_SIZE);
    for (uint32_t i = start; i < start + n; i++) bitmap_clear(i);
}

/* ── Heap allocator ──────────────────────────────────────────────────────── */

/* Every allocation is preceded by this header (16 bytes, aligned) */
typedef struct block_hdr {
    uint32_t         magic;     /* 0xCAFEBABE when valid          */
    uint32_t         size;      /* payload size in bytes           */
    bool             free;
    struct block_hdr *next;
    struct block_hdr *prev;
} block_hdr_t;

#define HEAP_MAGIC   0xCAFEBABEu
#define HDR_SIZE     sizeof(block_hdr_t)

static block_hdr_t *heap_head = NULL;
static size_t       used_bytes = 0;

/* Expand the heap by grabbing one more 4 KB page */
static block_hdr_t *heap_expand(size_t min_payload) {
    uint32_t pages = (uint32_t)((HDR_SIZE + min_payload + PAGE_SIZE - 1) / PAGE_SIZE);
    uintptr_t addr = phys_alloc_pages(pages);
    if (!addr) return NULL;

    block_hdr_t *blk = (block_hdr_t *)addr;
    blk->magic = HEAP_MAGIC;
    blk->size  = pages * PAGE_SIZE - (uint32_t)HDR_SIZE;
    blk->free  = true;
    blk->next  = NULL;
    blk->prev  = NULL;

    /* Append to linked list */
    if (!heap_head) {
        heap_head = blk;
    } else {
        block_hdr_t *cur = heap_head;
        while (cur->next) cur = cur->next;
        cur->next  = blk;
        blk->prev  = cur;
    }
    return blk;
}

/* Coalesce adjacent free blocks */
static void coalesce(block_hdr_t *blk) {
    /* Merge with next */
    if (blk->next && blk->next->free) {
        blk->size += (uint32_t)(HDR_SIZE + blk->next->size);
        blk->next  = blk->next->next;
        if (blk->next) blk->next->prev = blk;
    }
    /* Merge with previous */
    if (blk->prev && blk->prev->free) {
        blk->prev->size += (uint32_t)(HDR_SIZE + blk->size);
        blk->prev->next  = blk->next;
        if (blk->next) blk->next->prev = blk->prev;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void mm_init(void) {
    /* Clear bitmap */
    for (uint32_t i = 0; i < PAGE_COUNT / 32; i++) phys_bitmap[i] = 0;
    heap_head  = NULL;
    used_bytes = 0;

    /* Pre-allocate a few pages so the first kmalloc doesn't start cold */
    heap_expand(PAGE_SIZE * 4);
}

void *kmalloc(size_t size) {
    if (!size) return NULL;

    /* Align to 8 bytes */
    size = (size + 7) & ~7u;

    /* First-fit search */
    block_hdr_t *blk = heap_head;
    while (blk) {
        if (blk->free && blk->size >= size) {
            /* Split if remainder is large enough for a new block */
            if (blk->size >= size + HDR_SIZE + 8) {
                block_hdr_t *split = (block_hdr_t *)((uint8_t *)blk + HDR_SIZE + size);
                split->magic = HEAP_MAGIC;
                split->size  = blk->size - (uint32_t)size - (uint32_t)HDR_SIZE;
                split->free  = true;
                split->next  = blk->next;
                split->prev  = blk;
                if (blk->next) blk->next->prev = split;
                blk->next    = split;
                blk->size    = (uint32_t)size;
            }
            blk->free   = false;
            used_bytes += blk->size;
            return (void *)((uint8_t *)blk + HDR_SIZE);
        }
        blk = blk->next;
    }

    /* No suitable block — expand heap */
    blk = heap_expand(size);
    if (!blk) return NULL;
    return kmalloc(size);  /* retry after expansion */
}

void *kmalloc_aligned(size_t size, size_t align) {
    /* Allocate extra to guarantee we can find an aligned address */
    uint8_t *raw = (uint8_t *)kmalloc(size + align + sizeof(void *));
    if (!raw) return NULL;

    uintptr_t aligned = ((uintptr_t)raw + sizeof(void *) + align - 1) & ~(align - 1);
    /* Store the original pointer just before the returned address */
    ((void **)aligned)[-1] = raw;
    return (void *)aligned;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_hdr_t *blk = (block_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    if (blk->magic != HEAP_MAGIC) {
        vga_puts("[mm] kfree: bad magic — double free or corruption!\n");
        return;
    }
    if (blk->free) {
        vga_puts("[mm] kfree: block already free!\n");
        return;
    }
    used_bytes -= blk->size;
    blk->free   = true;
    coalesce(blk);
}

size_t mm_used(void) { return used_bytes; }
size_t mm_free(void) { return HEAP_SIZE - used_bytes; }

/* ── Paging (identity map first 8 MB, then enable paging) ────────────────── */

/* Page directory and first page table — must be 4 KB aligned */
static uint32_t page_directory[1024] ALIGN(4096);
static uint32_t page_table_low[1024] ALIGN(4096);   /* 0–4 MB */
static uint32_t page_table_hi[1024]  ALIGN(4096);   /* 4–8 MB */

void paging_init(void) {
    /* Identity-map the first 8 MB (covers kernel + heap) */
    for (int i = 0; i < 1024; i++) {
        /* Present | Read/Write */
        page_table_low[i] = (uint32_t)(i * PAGE_SIZE) | 0x03;
        page_table_hi[i]  = (uint32_t)((1024 + i) * PAGE_SIZE) | 0x03;
    }

    /* Clear page directory */
    for (int i = 0; i < 1024; i++) page_directory[i] = 0x00000002; /* not present */

    /* Install the two page tables */
    page_directory[0] = (uint32_t)page_table_low | 0x03;
    page_directory[1] = (uint32_t)page_table_hi  | 0x03;

    /* Load CR3 and enable paging via CR0 */
    __asm__ volatile (
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or  $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        :: "r"(page_directory) : "eax"
    );
}
