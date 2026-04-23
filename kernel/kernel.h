#ifndef KERNEL_H
#define KERNEL_H

#define PANIC(fmt, ...)                                                         \
    do {                                                                        \
         printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        while (1) {}                                                            \
    } while (0)

#endif /* KERNEL_H */
