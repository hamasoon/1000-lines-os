#ifndef COMMON_H
#define COMMON_H

#define TRUE    1
#define FALSE   0
#define NULL    ((void *) 0)

#define KB(x)   ((x) * 1024UL)
#define MB(x)   (KB(x) * 1024UL)
#define GB(x)   (MB(x) * 1024UL)

#define PACKED                      __attribute__((packed))
#define ALIGNED(x)                  __attribute__((aligned(x)))
// prevent the compiler from generating unexpected code for the function, 
// which is useful for context switching and interrupt handling code where we need precise control over the generated assembly instructions.
#define NAKED                       __attribute__((naked))               
#define UNUSED(x)                   (void)(x)
#define NORETURN                    __attribute__((noreturn))

/* System calls */
#define SYS_PUTCHAR 1
#define SYS_GETCHAR 2
#define SYS_EXIT    3
#define SYS_READ    4
#define SYS_WRITE   5

#define align_up(value, align)      __builtin_align_up(value, align)
#define ALIGN_UP(value, align)      (((value) + (align) - 1) & ~((align) - 1))
#define is_aligned(value, align)    __builtin_is_aligned(value, align)
#define offsetof(type, member)      __builtin_offsetof(type, member)
#define va_list                     __builtin_va_list
#define va_start                    __builtin_va_start
#define va_end                      __builtin_va_end
#define va_arg                      __builtin_va_arg

typedef int bool;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint64_t;
typedef unsigned long long uint64_t;
typedef uint64_t size_t;
typedef uint64_t paddr_t;
typedef uint64_t vaddr_t;

/* putchar is provided by each side: kernel uses SBI Console Putchar,
 * user provides its own (syscall-based) implementation. */
void putchar(char ch);
void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
void printf(const char *fmt, ...);

#endif /* COMMON_H */