#include "kernel.h"
#include "common.h"
#include "memory.h"
#include "exception.h"
#include "process.h"
#include "virtio.h"

/* Embedded shell image (linked in via shell.bin.o) */
extern char _binary_shell_bin_start[];
extern char _binary_shell_bin_size[];


/**
 * kernel_main - the main function of the kernel boot.S will jump to this function after setting up the stack pointer.
 */
void kernel_main(void) {
    init_memory();

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    virtio_init();

    char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, FALSE /* read from the disk */);
    printf("first sector: %s\n", buf);

    strcpy(buf, "hello from kernel!!!\n");
    read_write_disk(buf, 0, TRUE /* write to the disk */);

    idle_proc = create_process(NULL, 0); // updated!
    idle_proc->pid = 0; // idle
    current_proc = idle_proc;

    // new!
    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    yield();
    PANIC("switched to idle process");
}