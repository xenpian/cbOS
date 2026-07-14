#ifndef MM_H
#define MM_H

#include "types.h"

/* Physical memory layout */
#define HEAP_START   0x200000   /* 2 MB — safely above kernel */
#define HEAP_SIZE    0x400000   /* 4 MB heap */
#define PAGE_SIZE    4096

/* Simple bump allocator + free list */
void  mm_init(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t align);
void  kfree(void *ptr);

/* Paging */
void paging_init(void);

/* Memory stats */
size_t mm_used(void);
size_t mm_free(void);

#endif /* MM_H */
