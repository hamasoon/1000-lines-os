#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include "kernel.h"
#include "process.h"

#define USER_BASE 0x1000000

#define PAGE_SIZE           KB(4)
#define PAGE_OFFSET_BITS    12
#define PAGE_OFFSET_MASK    (PAGE_SIZE - 1)

#define PAGE_INDEX_BITS     9
#define PAGE_INDEX_MASK     ((1UL << PAGE_INDEX_BITS) - 1)
#define PAGE_TABLE_LEVELS   3
#define PAGE_TABLE_ENTRIES  (1UL << PAGE_INDEX_BITS)

/* PTE layout: [9:0] = flags, [53:10] = PPN, [63:54] = reserved.
 * PPN field starts at bit 10, not at the page offset boundary. */
#define PTE_PPN_SHIFT   10

#define GET_PTE_INDEX(va, level) \
    (((va) >> (PAGE_OFFSET_BITS + (level) * PAGE_INDEX_BITS)) & PAGE_INDEX_MASK)

#define SATP_SV39   (8UL << 60)     /* satp.MODE = 8 */
#define PAGE_V      (1 << 0)
#define PAGE_R      (1 << 1)
#define PAGE_W      (1 << 2)
#define PAGE_X      (1 << 3)
#define PAGE_U      (1 << 4)
/* A/D must be pre-set in software on hardware that does not update them. */
#define PAGE_A      (1 << 6)
#define PAGE_D      (1 << 7)

static paddr_t next_paddr;

paddr_t alloc_pages(uint64_t n);
void map_page(uint64_t *root_pt, vaddr_t vaddr, paddr_t paddr, uint64_t flags);
void init_page_table(process_t *proc);
void init_memory(void);
void enable_paging(void);

#endif /* MEMORY_H */
