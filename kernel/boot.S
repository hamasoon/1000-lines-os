/* OpenSBI (fw_dynamic) drops into S-mode here with:
 *   a0 = hart id
 *   a1 = flattened device tree (FDT) physical address
 * Secondary harts are parked; hart 0 tail-calls kernel_main, passing
 * a0/a1 through as lp64 arguments. */

.section ".text.boot"
.global boot

boot:
    bnez    a0, park

    la      sp, __stack_top
    tail    kernel_main

park:
    wfi
    j       park
