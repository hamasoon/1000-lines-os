#include "common.h"
#include "kernel.h"
#include "process.h"
#include "memory.h"

static process_t procs[PROCS_MAX];
static process_t idle_proc_storage;

process_t *current_proc = &idle_proc_storage;
process_t *idle_proc = &idle_proc_storage;

/* Saves callee-saved (ra, s0..s11) to the current kernel stack, records sp
 * in *prev_sp, then loads *next_sp and restores the same slots. The trailing
 * ret pops the next thread's ra -- for a fresh process that ra is user_entry
 * (or the kernel thread entry), otherwise the site that last called yield().
 * 13*8 = 104 is not 16-aligned, but switch_context calls no C function
 * internally and restores sp before ret, so the caller's alignment is kept. */
NAKED void switch_context(uint64_t *prev_sp, uint64_t *next_sp) {
    __asm__ __volatile__(
        "addi sp, sp, -13 * 8\n"
        "sd ra,  0  * 8(sp)\n"
        "sd s0,  1  * 8(sp)\n"
        "sd s1,  2  * 8(sp)\n"
        "sd s2,  3  * 8(sp)\n"
        "sd s3,  4  * 8(sp)\n"
        "sd s4,  5  * 8(sp)\n"
        "sd s5,  6  * 8(sp)\n"
        "sd s6,  7  * 8(sp)\n"
        "sd s7,  8  * 8(sp)\n"
        "sd s8,  9  * 8(sp)\n"
        "sd s9,  10 * 8(sp)\n"
        "sd s10, 11 * 8(sp)\n"
        "sd s11, 12 * 8(sp)\n"

        "sd sp, (a0)\n"
        "ld sp, (a1)\n"

        "ld ra,  0  * 8(sp)\n"
        "ld s0,  1  * 8(sp)\n"
        "ld s1,  2  * 8(sp)\n"
        "ld s2,  3  * 8(sp)\n"
        "ld s3,  4  * 8(sp)\n"
        "ld s4,  5  * 8(sp)\n"
        "ld s5,  6  * 8(sp)\n"
        "ld s6,  7  * 8(sp)\n"
        "ld s7,  8  * 8(sp)\n"
        "ld s8,  9  * 8(sp)\n"
        "ld s9,  10 * 8(sp)\n"
        "ld s10, 11 * 8(sp)\n"
        "ld s11, 12 * 8(sp)\n"
        "addi sp, sp, 13 * 8\n"
        "ret\n"
    );
}

/* Like create_process(NULL, 0) but the initial ra is the caller's entry
 * instead of user_entry, so the thread runs in S-mode without sret. */
process_t *create_kernel_thread(void (*entry)(void)) {
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

    init_page_table(proc);

    uint64_t *sp = (uint64_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                          /* s11 */
    *--sp = 0;                          /* s10 */
    *--sp = 0;                          /* s9  */
    *--sp = 0;                          /* s8  */
    *--sp = 0;                          /* s7  */
    *--sp = 0;                          /* s6  */
    *--sp = 0;                          /* s5  */
    *--sp = 0;                          /* s4  */
    *--sp = 0;                          /* s3  */
    *--sp = 0;                          /* s2  */
    *--sp = 0;                          /* s1  */
    *--sp = 0;                          /* s0  */
    *--sp = (uint64_t) entry;           /* ra  */

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint64_t) sp;
    return proc;
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

process_t *create_process(const void *image, size_t image_size) {
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

    init_page_table(proc);

    if (image != NULL) {
        const char *src = (const char *) image;
        for (uint64_t off = 0; off < image_size; off += PAGE_SIZE) {
            paddr_t page = alloc_pages(1);
            size_t remaining = image_size - off;
            size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
            memcpy((void *) page, src + off, copy_size);
            map_page(proc->page_table, USER_BASE + off, page,
                     PAGE_U | PAGE_R | PAGE_W | PAGE_X | PAGE_A | PAGE_D);
        }
    }

    uint64_t *sp = (uint64_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      /* s11 */
    *--sp = 0;                      /* s10 */
    *--sp = 0;                      /* s9  */
    *--sp = 0;                      /* s8  */
    *--sp = 0;                      /* s7  */
    *--sp = 0;                      /* s6  */
    *--sp = 0;                      /* s5  */
    *--sp = 0;                      /* s4  */
    *--sp = 0;                      /* s3  */
    *--sp = 0;                      /* s2  */
    *--sp = 0;                      /* s1  */
    *--sp = 0;                      /* s0  */
    *--sp = (uint64_t) user_entry;  /* ra  */

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint64_t) sp;
    return proc;
}

static process_t *scheduler(void) {
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

/* sfence.vma brackets satp to flush stale TLB entries and drain any in-flight
 * page-table walks. sscratch points at the next process's kernel stack top
 * so kernel_entry's csrrw sp,sscratch,sp lands on the correct stack. */
void yield(void) {
    process_t *next = scheduler();
    if (next == current_proc)
        return;

    uint64_t satp = SATP_SV39 |
                    ((uint64_t) next->page_table >> PAGE_OFFSET_BITS);
    uint64_t sscratch = (uint64_t) &next->stack[sizeof(next->stack)];

    __asm__ __volatile__(
        "sfence.vma zero, zero\n"
        "csrw satp, %[satp]\n"
        "sfence.vma zero, zero\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (satp), [sscratch] "r" (sscratch)
        : "memory"
    );

    process_t *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}
