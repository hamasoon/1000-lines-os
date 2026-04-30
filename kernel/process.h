#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"

#define MAX_CPUS 4
#define MAX_PROCS 256

#define PROC_UNUSED     0
#define PROC_RUNNING    1
#define PROC_RUNNABLE   2
#define PROC_EXITED     3

#define PROC_STACK_SIZE 8192

/* Context structure for saving/restoring process state */
typedef struct __context {
    uint64_t ra;
    uint64_t sp;

    // callee-saved registers (s0-s11)
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
} context_t;

/* ALIGNED(16) on `stack` makes the compiler pad the field so that
 * &stack[sizeof(stack)] is 16B-aligned (required for lp64 SP at call
 * boundaries). It also bumps process_t's own alignment to 16, so every
 * element of `static process_t procs[]` stays aligned. */
typedef struct __process {
    int pid;
    int state;
    vaddr_t sp;
    uint64_t *page_table;
    uint8_t stack[PROC_STACK_SIZE] ALIGNED(16);
} process_t;

typedef struct __cpu {
    struct __process *proc;
    context_t context;
    uint64_t hart_id;
} cpu_t;

extern process_t *current_proc;
extern process_t *idle_proc;

cpu_t *get_cpu(void);
void switch_context(uint64_t *prev_sp, uint64_t *next_sp);
process_t *create_process(const void *image, size_t image_size);
process_t *create_kernel_thread(void (*entry)(void));
void yield(void);

#endif /* PROCESS_H */
