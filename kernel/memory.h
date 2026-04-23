#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include "kernel.h"
#include "process.h"

#define USER_BASE 0x1000000

#define PAGE_SIZE   KB(4)
#define GET_PTE_INDEX(va, level) (((va) >> (12 + (level) * 10)) & 0x3ff) // Get the page table index for a given virtual address and page table level

#define SATP_SV32   (1u << 31)  // Indicates that the page table is in SV32 mode
#define PAGE_V      (1 << 0)    // Valid bit: indicates whether the page table entry is valid
#define PAGE_R      (1 << 1)    // Readable bit: indicates whether the page is readable
#define PAGE_W      (1 << 2)    // Writable bit: indicates whether the page is writable
#define PAGE_X      (1 << 3)    // Executable bit: indicates whether the page is executable
#define PAGE_U      (1 << 4)    // User bit: indicates whether the page is accessible from user mode
#define PAGE_A      (1 << 6)    // Accessed bit (must be pre-set by SW when HW doesn't auto-update)
#define PAGE_D      (1 << 7)    // Dirty bit (must be pre-set by SW for writable pages when HW doesn't auto-update)

static paddr_t next_paddr;

paddr_t alloc_pages(uint64_t n);
void map_page(uint64_t *table1, vaddr_t vaddr, paddr_t paddr, uint64_t flags);
void init_page_table(process_t *proc);
void init_memory(void);

#endif /* MEMORY_H */