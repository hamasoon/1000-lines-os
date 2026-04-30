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
#define SSTATUS_SPIE            (1ULL << 5)
#define SSTATUS_UBE             (1ULL << 6)
#define SSTATUS_SPP             (1ULL << 8)
#define SSTATUS_SUM             (1ULL << 18)
#define SSTATUS_MXR             (1ULL << 19)

/* Interrupt codes */
#define IRQ_S_SOFT           1
#define IRQ_M_SOFT           3
#define IRQ_S_TIMER          5
#define IRQ_S_EXT            9

/* Exception codes */
#define EXC_INST_MISALIGNED     0
#define EXC_INST_ACCESS         1
#define EXC_ILLEGAL_INST        2
#define EXC_BREAKPOINT          3
#define EXC_LOAD_MISALIGNED     4
#define EXC_LOAD_ACCESS         5
#define EXC_STORE_MISALIGNED    6
#define EXC_STORE_ACCESS        7
#define EXC_U_ECALL             8
#define EXC_S_ECALL             9
#define EXC_M_ECALL             11
#define EXC_INST_PAGE_FAULT     12
#define EXC_LOAD_PAGE_FAULT     13
#define EXC_STORE_PAGE_FAULT    15

/* Check interrupt or exception */
#define SCAUSE_INTERRUPT        (1ULL << 63)
#define SCAUSE_CODE_MASK        (~SCAUSE_INTERRUPT)

/* 31 GPRs + sstatus = 32 * 8B = 256B (16B aligned). sstatus must be saved
 * and restored across the trap because nested traps via context switches
 * (e.g. timer preempts kernel -> switch to user proc -> user ecall ->
 * switch back) leave the LIVE sstatus reflecting the most-recent trap's
 * SPP. Without per-trap-frame sstatus, the outer trap's sret would use
 * the inner trap's SPP and return to the wrong privilege mode. */
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
    uint64_t sstatus;   /* sstatus at trap entry; restored before sret */
} trap_frame_t;

void kernel_entry(void);
void handle_syscall(trap_frame_t *f);
void handle_interrupt(uint64_t code);
void handle_trap(trap_frame_t *f);

#endif /* EXCEPTION_H */
