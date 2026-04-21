#ifndef KERNEL_H
#define KERNEL_H

/**
 * @brief PANIC - Macro to print a panic message and halt the system
 * 
 * @param fmt Format string for the panic message, followed by optional arguments
 * 
 * This macro uses the printf function to output a panic message that includes the file name 
 * and line number where the panic occurred, along with a custom message provided by the caller. 
 * After printing the message, it enters an infinite loop to halt the system, preventing any further execution.
 */
#define PANIC(fmt, ...)                                                         \
    do {                                                                        \
         printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        while (1) {}                                                            \
    } while (0)
#endif // KERNEL_H
