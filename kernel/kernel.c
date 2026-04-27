#include "kernel.h"
#include "common.h"
#include "memory.h"
#include "exception.h"
#include "process.h"
#include "virtio.h"
#include "file.h"
#include "sbi.h"
#include "time.h"

/* objcopy also emits _binary_shell_bin_size, but that is an ABS symbol whose
 * value equals the size. Under -mcmodel=medany the compiler reaches it via
 * pcrel auipc+addi, and the offset overflows R_RISCV_PCREL_HI20. Derive the
 * size from end-start instead. */
extern char _binary_shell_bin_start[];
extern char _binary_shell_bin_end[];

void kernel_main(unsigned long hartid, unsigned long fdt) {
    init_memory();
    WRITE_CSR(stvec, (uint64_t) kernel_entry);
    /* sscratch = 0 marks "running in S-mode" -- the trap entry uses this to
     * pick between the U-mode entry path (swap to kernel_top) and the S-mode
     * entry path (push the trap frame below the in-use kernel stack). */
    WRITE_CSR(sscratch, 0);

    /* Unmask the supervisor timer source and arm the first deadline.
     * user_entry() sets sstatus.SPIE on each U-mode entry, so user code runs
     * with sstatus.SIE=1; STIE then lets the timer reach the trap handler.
     * handle_interrupt() rearms on every tick. */
    SET_CSR(sie, SIE_STIE);
    sbi_set_timer(read_time() + TIMER_INTERVAL);

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

	sfs_init();
	printf("sfs initialized.\n");

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

    /* Phase 7: kernel preemption. From here on the kernel itself runs with
     * sstatus.SIE=1, so a timer can preempt S-mode code (e.g. the wfi loop
     * below or another kernel thread). yield() saves/disables/restores SIE
     * across its critical section; the trap handler runs with SIE=0
     * (hardware-cleared on entry) and sret restores SIE from SPIE. */
    SET_CSR(sstatus, SSTATUS_SIE);

    yield();

    printf("Shell exited. Idle resumed.\n");

    for (;;)
        __asm__ __volatile__("wfi");
}
