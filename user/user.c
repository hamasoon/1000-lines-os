#include "user.h"
#include "common.h"

extern char __stack_top[];

int syscall(int sysno, int arg0, int arg1, int arg2) {
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

void putchar(char ch) {
    syscall(SYS_PUTCHAR, ch, 0, 0);
}

int getchar(void) {
    return syscall(SYS_GETCHAR, 0, 0, 0);
}

NORETURN void exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
    for (;;); // Just in case!
}

int read(const char *filename, char *buf, int len) {
    return syscall(SYS_READ, (int) filename, (int) buf, len);
}

int write(const char *filename, const char *buf, int len) {
    return syscall(SYS_WRITE, (int) filename, (int) buf, len);
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