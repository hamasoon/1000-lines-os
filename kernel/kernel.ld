ENTRY(boot)                                 /* Define the entry point of the kernel --> boot is the function where the kernel starts executing */

SECTIONS {
    . = 0x80200000;                         /* Base address of the kernel in memory */
    __kernel_base = .;                      /* Define a symbol for the base address of the kernel */ 

    .text :{                                /* Code section. allocate .text section and .text.* sections here */
        KEEP(*(.text.boot));
        *(.text .text.*);
    }

    .rodata : ALIGN(4) {                    /* Read-only data section */
        *(.rodata .rodata.*);
    }

    .data : ALIGN(4) {                      /* Data section */
        *(.data .data.*);
    }

    .bss : ALIGN(4) {                       /* Uninitialized data section */
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    . = ALIGN(4);                           /* Align to 4 bytes for the stack */
    . += 128 * 1024; /* 128KB */            /* Allocate 128KB for the stack */
    __stack_top = .;                        /* Define the top of the stack(+128KB from the base address) */

    . = ALIGN(4096);                        /* Align to 4KB for the free RAM */  
    __free_ram = .;                         /* Define the start of the free RAM (after the kernel and stack) */
    . += 64 * 1024 * 1024;                  /* 64MB */
    __free_ram_end = .;                     /* Define the end of the free RAM */
}