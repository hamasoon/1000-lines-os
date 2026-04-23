#include "user.h"
#include "common.h"

extern char __stack_top[];

/* ecall ABI (matches kernel/exception.c handle_syscall):
 *   a3 = syscall number
 *   a0..a2 = args
 *   return: a0
 * All slots are `long` so that 64-bit pointers survive lp64 without being
 * truncated into 32-bit int. */
long syscall(long sysno, long arg0, long arg1, long arg2) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = sysno;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

void putchar(char ch) {
    syscall(SYS_PUTCHAR, (long) ch, 0, 0);
}

int getchar(void) {
    return (int) syscall(SYS_GETCHAR, 0, 0, 0);
}

NORETURN void exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
    for (;;);
}

long read(const char *filename, char *buf, long len) {
    return syscall(SYS_READ, (long) filename, (long) buf, len);
}

long write(const char *filename, const char *buf, long len) {
    return syscall(SYS_WRITE, (long) filename, (long) buf, len);
}

NAKED
__attribute__((section(".text.start")))
void start(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top] \n"
        "call main           \n"
        "call exit           \n"
        :: [stack_top] "r" (__stack_top)
    );
}
