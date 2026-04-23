/* User-space linker script. The shell is placed at USER_BASE (0x1000000);
 * the kernel copies the flat binary one page at a time into a fresh page
 * table when the process is created. */
ENTRY(start)

SECTIONS {
    . = 0x1000000;

    .text : {
        KEEP(*(.text.start));
        *(.text .text.*);
    }

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*);
    }

    .data : ALIGN(8) {
        *(.data .data.*);
    }

    .bss : ALIGN(8) {
        *(.bss .bss.* .sbss .sbss.*);

        /* psABI lp64: SP 16B-aligned at call boundaries. */
        . = ALIGN(16);
        . += 64 * 1024;
        __stack_top = .;

        ASSERT(. < 0x1800000, "too large executable");
    }
}
