/* OpenSBI (fw_dynamic) runs at 0x80000000 and drops into S-mode at our entry.
 * QEMU virt with -kernel loads the ELF at 0x80200000 by convention. */
ENTRY(boot)

SECTIONS {
    . = 0x80200000;
    __kernel_base = .;

    .text : {
        KEEP(*(.text.boot));
        *(.text .text.*);
    }

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*);
    }

    .data : ALIGN(8) {
        *(.data .data.*);
    }

    .bss : ALIGN(8) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    /* Boot stack (psABI lp64: SP must be 16B-aligned at call boundaries). */
    . = ALIGN(16);
    . += 128 * 1024;
    __stack_top = .;

    /* Free RAM pool used by the kernel page allocator. */
    . = ALIGN(4096);
    __free_ram = .;
    . += 256 * 1024 * 1024;
    __free_ram_end = .;
}
