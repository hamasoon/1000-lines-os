#ifndef EXCEPTION_H
#define EXCEPTION_H

#include "common.h"

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint64_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)

/* csrs/csrc atomically set/clear bits without a read-modify-write race. */
#define SET_CSR(reg, mask)                                                     \
    do {                                                                       \
        uint64_t __tmp = (mask);                                               \
        __asm__ __volatile__("csrs " #reg ", %0" ::"r"(__tmp));                \
    } while (0)

#define CLEAR_CSR(reg, mask)                                                   \
    do {                                                                       \
        uint64_t __tmp = (mask);                                               \
        __asm__ __volatile__("csrc " #reg ", %0" ::"r"(__tmp));                \
    } while (0)

/* sie / sip bit positions. */
#define SIE_STIE                (1ULL << 5)

/* sstatus bit positions used by the kernel. SSTATUS_SPIE lives in process.h
 * for legacy reasons; the trap path needs SIE and SPP here. */
#define SSTATUS_SIE             (1ULL << 1)
#define SSTATUS_SPP             (1ULL << 8)

#define SCAUSE_S_TIMER          5
#define SCAUSE_ECALL            8
#define SCAUSE_INTERRUPT        (1ULL << 63)
#define SCAUSE_CODE_MASK        (~SCAUSE_INTERRUPT)

/* 31 GPRs * 8B = 248B, padded to 256B (16B aligned). lp64 psABI requires
 * SP 16B-aligned at function call boundaries; 248B alone would leave
 * handle_trap()'s entry SP off by 8. */
typedef struct __trap_frame {
    uint64_t ra;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t sp;        /* trapping sp (user or kernel) */
    uint64_t _pad;      /* alignment only -- never touched by asm */
} trap_frame_t;

void kernel_entry(void);
void handle_syscall(trap_frame_t *f);
void handle_interrupt(uint64_t code);
void handle_trap(trap_frame_t *f);

#endif /* EXCEPTION_H */
