#ifndef PROCESS_H
#define PROCESS_H

#include "common.h"

#define SSTATUS_SPIE (1 << 5)

#define PROCS_MAX 8         // maximum number of processes

/* process states */
#define PROC_UNUSED     0   // process slot is unused
#define PROC_RUNNING    1   // process is currently running
#define PROC_RUNNABLE   2   // process is runnable (ready to run)
#define PROC_EXITED     3   // process has exited

#define PROC_STACK_SIZE 8192 // 8 KB stack size for each process

/**
 * @brief struct process - represents a process in the system
 * @pid: process ID
 * @state: current state of the process (e.g., unused, runnable)
 * @sp: stack pointer for the process
 * @page_table: page table for the process 
 * @stack: memory allocated for the process's stack
 * 
 * This structure is used to manage processes in the operating system. Each process has a unique ID, 
 * a state that indicates whether it is currently in use or runnable, 
 * a stack pointer for managing function calls and local variables, 
 * and a fixed-size stack for storing the process's execution context.
 */
typedef struct __process {
    int pid;                            // process ID
    int state;                          // current state of the process (e.g., unused, runnable)
    vaddr_t sp;                         // stack pointer for the process
    uint64_t* page_table;               // page table for the process
    uint8_t stack[PROC_STACK_SIZE];     // memory allocated for the process's stack (8 KB)
} process_t;

extern process_t *current_proc;
extern process_t *idle_proc;

void switch_context(uint64_t *prev_sp, uint64_t *next_sp);
process_t *create_process(const void *image, size_t image_size);
void yield(void);

#endif /* PROCESS_H */