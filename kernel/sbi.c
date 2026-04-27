#include "sbi.h"

/* SBI (Supervisor Binary Interface) lets S-mode call into M-mode firmware
 * via ecall. Args go in a0..a5, function ID in a6, extension ID in a7.
 * Return comes back in a0 (error) and a1 (value); all other callee-saved
 * registers are preserved by the firmware. */
sbiret_t sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                  long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");

    return (sbiret_t){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

long getchar(void) {
    sbiret_t ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2 /* Console Getchar */);
    return ret.error;
}

/* Legacy Set Timer (EID 0x00). Programs the next S-mode timer interrupt to
 * fire when the `time` CSR reaches stime_value. On RV64 the full 64-bit
 * value is passed in a0; the firmware also clears any pending STIP. */
void sbi_set_timer(uint64_t stime_value) {
    sbi_call((long)stime_value, 0, 0, 0, 0, 0, 0, 0x00 /* Set Timer */);
}
