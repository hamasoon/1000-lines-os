#include "common.h"
#include "kernel.h"
#include "process.h"
#include "memory.h"

static process_t procs[PROCS_MAX];              // process table to hold all processes in the system
static process_t idle_proc_storage;             // storage for the idle/boot context

process_t *current_proc = &idle_proc_storage;   // currently running process (boot context at start)
process_t *idle_proc = &idle_proc_storage;      // sentinel representing the idle/boot context

/**
 * @brief switch_context - context switch between two processes by saving and restoring their CPU state.
 * @prev_sp: pointer to the stack pointer of the currently running process (to be saved)
 * @next_sp: pointer to the stack pointer of the next process to run (to be loaded)
 * 
 * This function performs a context switch between two processes by saving the CPU state of the currently 
 * running process and restoring the CPU state of the next process to run. It saves the 
 * callee-saved registers (ra, s0-s11) of the current process onto its stack,
 * saves the current stack pointer to the current process's sp field, 
 * and loads the next process's sp into the stack pointer.
 */
NAKED void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__(
        // Save callee-saved registers of the current process onto its stack
        "addi sp, sp, -13 * 4\n" // 13 callee-saved registers (ra, s0-s11) each 4 bytes
        "sw ra,  0  * 4(sp)\n"   // callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Save the current stack pointer to the current process's sp field and load the next process's sp into the stack pointer
        "sw sp, (a0)\n"         // *prev_sp = sp; save current process's stack pointer
        "lw sp, (a1)\n"         // switch to next process's stack pointer: sp = *next_sp;

        // Restore callee-saved registers of the next process from its stack
        "lw ra,  0  * 4(sp)\n"  
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n" 
        "ret\n"
    );
}

NAKED void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE)
    );
}

/**
 * @brief Creates a new process with the given user image and entry point.
 * @image: Pointer to the user program's binary image in memory.
 * @image_size: Size of the user program's binary image in bytes.
 * @return Pointer to the newly created process control block (PCB) for the new process.
 * 
 * This function is responsible for creating a new process in the operating system. 
 * It first searches for an unused process slot in the process table. If a user image is provided, 
 * it allocates memory pages for the process, copies the user program into those pages, 
 * and maps them into the process's address space at a predefined base address (USER_BASE). 
 * The function then prepares the initial stack for the new process with default register values 
 * and sets up the process control block (PCB) with the new process's ID, state, stack pointer, 
 * and page table. Finally, it returns a pointer to the newly created PCB.
 */
process_t *create_process(const void *image, size_t image_size) {
    // find an unused process slot
    process_t *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    // allocate a page for the process's page table and map the kernel into it
    init_page_table(proc);

    // If a user image was provided, allocate per-process pages, copy the image
    // into them, and map them at USER_BASE. The entry PC is then USER_BASE.
    if (image != NULL) {
        const char *src = (const char *) image;
        for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
            paddr_t page = alloc_pages(1);
            size_t remaining = image_size - off;
            size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
            memcpy((void *) page, src + off, copy_size);
            map_page(proc->page_table, USER_BASE + off, page,
                     PAGE_U | PAGE_R | PAGE_W | PAGE_X | PAGE_A | PAGE_D);
        }
    }

    // prepare the kernel stack with initial register values for the new process
    // at the first context switch, switch_context will restore these values
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra (changed!)

    // initialize the process control block
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    return proc;
}

/**
 * @brief Simple round-robin scheduler to select the next runnable process.
 * @return Pointer to the next process to run, or the idle process if no other processes are runnable.
 * 
 * This function implements a simple round-robin scheduling algorithm to select the next process to run. 
 * It starts scanning the process table from the slot immediately following the currently running process. 
 * If it finds a process in the PROC_RUNNABLE state, it returns a pointer to that process. 
 * If no runnable processes are found after scanning the entire table, it returns a pointer to the idle process, 
 * which is a special process that runs when no other processes are runnable.
 */
static process_t *scheduler(void) {
    // round-robin: start scanning from the slot after the currently running one
    int start_idx = (current_proc == idle_proc)
                    ? 0
                    : (int)(current_proc - procs + 1);

    for (int i = 0; i < PROCS_MAX; i++) {
        process_t *p = &procs[(start_idx + i) % PROCS_MAX];
        if (p->state == PROC_EXITED)
            p->state = PROC_UNUSED;
        if (p != idle_proc && p->state == PROC_RUNNABLE)
            return p;
    }
    return idle_proc;
}

/**
 * @brief yield - yields the CPU to allow another process to run. 
 * 
 * This function is called by a running process to voluntarily give up the CPU so that another process can run. 
 * It first calls the scheduler to select the next process to run. If the next process is the same as the current process,
 * it simply returns without doing anything. Otherwise, it performs a context switch to the next process 
 * by saving the current process's CPU state,
 */
void yield(void) {
    process_t *next = scheduler();
    if (next == current_proc)
        return;

    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    process_t *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}