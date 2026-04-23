#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"

#define SSTATUS_SPIE (1 << 5)

#define PROCS_MAX 8

#define PROC_UNUSED     0
#define PROC_RUNNING    1
#define PROC_RUNNABLE   2
#define PROC_EXITED     3

#define PROC_STACK_SIZE 8192

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

extern process_t *current_proc;
extern process_t *idle_proc;

void switch_context(uint64_t *prev_sp, uint64_t *next_sp);
process_t *create_process(const void *image, size_t image_size);
process_t *create_kernel_thread(void (*entry)(void));
void yield(void);

#endif /* PROCESS_H */
