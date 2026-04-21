.section ".text.boot"
.global boot

boot:                   // entry point defined in linker script(start point of the kernel)
    la sp, __stack_top  // Set the stack pointer to the top of the stack    
    j  kernel_main      // Jump to the main kernel function
