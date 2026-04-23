#include "memory.h"
#include "virtio.h"

extern char __kernel_base[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char __bss[], __bss_end[];

paddr_t alloc_pages(uint64_t n) {
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

void map_page(uint64_t *root_pt, vaddr_t va, paddr_t pa, uint64_t flags) {
    if (!is_aligned(va, PAGE_SIZE))
        PANIC("unaligned vaddr %llx", (uint64_t) va);
    if (!is_aligned(pa, PAGE_SIZE))
        PANIC("unaligned paddr %llx", (uint64_t) pa);

    uint64_t *pt = root_pt;
    for (int level = PAGE_TABLE_LEVELS - 1; level > 0; level--) {
        uint64_t idx = GET_PTE_INDEX(va, level);
        uint64_t pte = pt[idx];
        if (pte & PAGE_V) {
            pt = (uint64_t *) ((pte >> PTE_PPN_SHIFT) * PAGE_SIZE);
        } else {
            uint64_t *next = (uint64_t *) alloc_pages(1);
            pt[idx] = ((uint64_t) next / PAGE_SIZE) << PTE_PPN_SHIFT | PAGE_V;
            pt = next;
        }
    }

    uint64_t leaf_idx = GET_PTE_INDEX(va, 0);
    uint64_t ppn = pa / PAGE_SIZE;
    pt[leaf_idx] = (ppn << PTE_PPN_SHIFT) | flags | PAGE_V;
}

static void map_kernel(uint64_t *table) {
    for (paddr_t paddr = (paddr_t) __kernel_base; paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X | PAGE_A | PAGE_D);
}

static void map_virtio(uint64_t *table) {
    map_page(table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);
}

void init_page_table(process_t *proc) {
    proc->page_table = (uint64_t *) alloc_pages(1);
    memset(proc->page_table, 0, PAGE_SIZE);
    map_kernel(proc->page_table);
    map_virtio(proc->page_table);
}

void init_memory(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
    next_paddr = (paddr_t) __free_ram;
}

/* The root PT itself is allocated from free_ram, so it is covered by the
 * identity map and the page walker can still read it after paging is on.
 * sfence.vma surrounds the satp write to flush stale TLB/PTW state. */
void enable_paging(void) {
    uint64_t *root = (uint64_t *) alloc_pages(1);
    memset(root, 0, PAGE_SIZE);

    for (paddr_t pa = (paddr_t) __kernel_base;
         pa < (paddr_t) __free_ram_end;
         pa += PAGE_SIZE) {
        map_page(root, pa, pa,
                 PAGE_R | PAGE_W | PAGE_X | PAGE_A | PAGE_D);
    }

    /* virtio MMIO: init_page_table adds this to per-process PTs, but the
     * kernel root PT needs it too so virtio_init() can run before any
     * process is scheduled. */
    map_page(root, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR,
             PAGE_R | PAGE_W | PAGE_A | PAGE_D);

    uint64_t satp = SATP_SV39 | ((uint64_t) root >> PAGE_OFFSET_BITS);
    __asm__ __volatile__(
        "sfence.vma zero, zero\n"
        "csrw satp, %0\n"
        "sfence.vma zero, zero\n"
        :
        : "r"(satp)
        : "memory"
    );
}
