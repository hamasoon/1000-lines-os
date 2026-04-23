#include "kernel.h"
#include "common.h"
#include "memory.h"
#include "exception.h"
#include "process.h"
#include "virtio.h"

/* objcopy also emits _binary_shell_bin_size, but that is an ABS symbol whose
 * value equals the size. Under -mcmodel=medany the compiler reaches it via
 * pcrel auipc+addi, and the offset overflows R_RISCV_PCREL_HI20. Derive the
 * size from end-start instead. */
extern char _binary_shell_bin_start[];
extern char _binary_shell_bin_end[];

void kernel_main(unsigned long hartid, unsigned long fdt) {
    init_memory();
    WRITE_CSR(stvec, (uint64_t) kernel_entry);

    printf("Hello from riscv64 S-mode! (paging off)\n");
    printf("  hartid = %lu\n", (unsigned long) hartid);
    printf("  fdt    = %p\n", (void *) fdt);

    enable_paging();
    printf("Sv39 paging ON.\n");
    printf("  satp = 0x%llx\n", (uint64_t) READ_CSR(satp));

    virtio_init();
    printf("virtio-blk: capacity = %lu sectors (%lu bytes)\n",
           (unsigned long) (virtio_blk_get_capacity() / SECTOR_SIZE),
           (unsigned long) virtio_blk_get_capacity());

    char buf[SECTOR_SIZE];
    read_disk(buf, 0);
    buf[64] = '\0';
    printf("first 64B of sector 0: %s\n", buf);

    memset(buf, 0, SECTOR_SIZE);
    strcpy(buf, "hello from riscv64 kernel!\n");
    write_disk(buf, 0);
    printf("wrote back to sector 0\n");

    /* idle takes the boot context: create_process sets ra to user_entry, but
     * the first yield() overwrites idle's sp with the current (boot) sp before
     * that ra is ever reached, so user_entry is harmless here. */
    idle_proc = create_process(NULL, 0);
    idle_proc->pid = 0;
    current_proc = idle_proc;

    size_t shell_size = (size_t) (_binary_shell_bin_end - _binary_shell_bin_start);
    printf("shell image: %p .. %p (%lu bytes)\n",
           (void *) _binary_shell_bin_start,
           (void *) _binary_shell_bin_end,
           (unsigned long) shell_size);

    create_process(_binary_shell_bin_start, shell_size);
    printf("Yielding to shell (user-mode)...\n");

    yield();

    printf("Shell exited. Idle resumed.\n");

    for (;;)
        __asm__ __volatile__("wfi");
}
