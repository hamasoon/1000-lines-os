#include "memory.h"
#include "virtio.h"

extern char __kernel_base[], __stack_top[];     // symbols defined in the linker script
extern char __free_ram[], __free_ram_end[];
extern char __bss[], __bss_end[];

paddr_t alloc_pages(uint32_t n) {
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t ppn = paddr / PAGE_SIZE;

    uint32_t idx1 = GET_PTE_INDEX(vaddr, 1);
    uint32_t pte1 = table1[idx1];
    uint32_t *table0;
    if (pte1 & PAGE_V) {
        table0 = (uint32_t *) ((pte1 >> 10) * PAGE_SIZE);
    } else {
        table0 = (uint32_t *) alloc_pages(1);
        table1[idx1] = ((uint32_t) table0 / PAGE_SIZE) << 10 | PAGE_V;
    }
    uint32_t idx0 = GET_PTE_INDEX(vaddr, 0);
    table0[idx0] = (ppn << 10) | flags | PAGE_V;
}

static void map_kernel(uint32_t *table) {
    for (paddr_t paddr = (paddr_t) __kernel_base; paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X | PAGE_A | PAGE_D);
}

static void map_virtio(uint32_t *table) {
    map_page(table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W); // new
}

void init_page_table(process_t *proc) {
    proc->page_table = (uint32_t *) alloc_pages(1);
    memset(proc->page_table, 0, PAGE_SIZE);
    map_kernel(proc->page_table);
    map_virtio(proc->page_table);
}

void init_memory(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
    next_paddr = (paddr_t) __free_ram;
}