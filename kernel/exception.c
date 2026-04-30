#include "exception.h"
#include "kernel.h"
#include "sbi.h"
#include "process.h"
#include "time.h"

/* Trap entry. sscratch invariant:
 *   sscratch == 0           : we were running in S-mode (kernel preemption)
 *   sscratch == kernel_top  : we were running in U-mode; kernel_top is the
 *                             current proc's kernel stack top, used as the
 *                             trap frame anchor.
 * The two cases land on different stacks:
 *   U-mode entry: swap sp<->sscratch puts sp = kernel_top (empty stack), so
 *                 the trap frame goes at the very top.
 *   S-mode entry: the swap leaves sp = 0 because sscratch was 0; we restore
 *                 the original kernel sp from sscratch and push the trap frame
 *                 BELOW the in-use kernel stack (which is what makes kernel
 *                 preemption safe -- the original kernel data above is intact).
 * On the way out, sscratch is rearmed based on sstatus.SPP: kernel_top for a
 * U-mode return, 0 for an S-mode return. */
NAKED
ALIGNED(4)
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrrw sp, sscratch, sp\n"
        /* sp=0 iff sscratch was 0 (S-mode entry). */
        "bnez sp, 1f\n"
        "csrr sp, sscratch\n"           /* restore kernel sp; sscratch still=kernel_sp */
        "1:\n"

        "addi sp, sp, -8 * 32\n"
        "sd ra,  8 * 0(sp)\n"
        "sd gp,  8 * 1(sp)\n"
        "sd tp,  8 * 2(sp)\n"
        "sd t0,  8 * 3(sp)\n"
        "sd t1,  8 * 4(sp)\n"
        "sd t2,  8 * 5(sp)\n"
        "sd t3,  8 * 6(sp)\n"
        "sd t4,  8 * 7(sp)\n"
        "sd t5,  8 * 8(sp)\n"
        "sd t6,  8 * 9(sp)\n"
        "sd a0,  8 * 10(sp)\n"
        "sd a1,  8 * 11(sp)\n"
        "sd a2,  8 * 12(sp)\n"
        "sd a3,  8 * 13(sp)\n"
        "sd a4,  8 * 14(sp)\n"
        "sd a5,  8 * 15(sp)\n"
        "sd a6,  8 * 16(sp)\n"
        "sd a7,  8 * 17(sp)\n"
        "sd s0,  8 * 18(sp)\n"
        "sd s1,  8 * 19(sp)\n"
        "sd s2,  8 * 20(sp)\n"
        "sd s3,  8 * 21(sp)\n"
        "sd s4,  8 * 22(sp)\n"
        "sd s5,  8 * 23(sp)\n"
        "sd s6,  8 * 24(sp)\n"
        "sd s7,  8 * 25(sp)\n"
        "sd s8,  8 * 26(sp)\n"
        "sd s9,  8 * 27(sp)\n"
        "sd s10, 8 * 28(sp)\n"
        "sd s11, 8 * 29(sp)\n"

        /* slot 30 = trapping sp. sscratch holds it in both cases:
         * U-mode entry -> user_sp (loaded by csrrw); S-mode entry -> kernel_sp
         * (we never overwrote sscratch in the bnez branch). */
        "csrr a0, sscratch\n"
        "sd a0,  8 * 30(sp)\n"
        /* slot 31 = sstatus at trap entry. Saved so that nested traps via
         * context switches don't clobber THIS trap's SPP/SPIE before sret. */
        "csrr a0, sstatus\n"
        "sd a0,  8 * 31(sp)\n"
        /* Mark "now in S-mode" so any nested entry detects it. Hardware keeps
         * sstatus.SIE=0 during the handler, so no nested trap should occur,
         * but the invariant must hold across the handler regardless. */
        "csrw sscratch, zero\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        /* Restore sstatus from the trap frame so sret sees THIS trap's
         * SPP/SPIE, not whatever a nested trap left in the live CSR. */
        "ld t0,  8 * 31(sp)\n"
        "csrw sstatus, t0\n"
        /* Decide sscratch based on the saved sstatus.SPP. SPP=0 -> returning
         * to U-mode, rearm sscratch with the kernel_top of the current trap
         * frame (= sp + 256, since the trap frame sits exactly at the top
         * for U-mode entries). SPP=1 -> returning to S-mode, leave sscratch=0. */
        "andi t0, t0, 0x100\n"           /* bit 8 = SPP */
        "bnez t0, 2f\n"
        "addi t0, sp, 8 * 32\n"
        "csrw sscratch, t0\n"
        "2:\n"

        "ld ra,  8 * 0(sp)\n"
        "ld gp,  8 * 1(sp)\n"
        "ld tp,  8 * 2(sp)\n"
        "ld t0,  8 * 3(sp)\n"
        "ld t1,  8 * 4(sp)\n"
        "ld t2,  8 * 5(sp)\n"
        "ld t3,  8 * 6(sp)\n"
        "ld t4,  8 * 7(sp)\n"
        "ld t5,  8 * 8(sp)\n"
        "ld t6,  8 * 9(sp)\n"
        "ld a0,  8 * 10(sp)\n"
        "ld a1,  8 * 11(sp)\n"
        "ld a2,  8 * 12(sp)\n"
        "ld a3,  8 * 13(sp)\n"
        "ld a4,  8 * 14(sp)\n"
        "ld a5,  8 * 15(sp)\n"
        "ld a6,  8 * 16(sp)\n"
        "ld a7,  8 * 17(sp)\n"
        "ld s0,  8 * 18(sp)\n"
        "ld s1,  8 * 19(sp)\n"
        "ld s2,  8 * 20(sp)\n"
        "ld s3,  8 * 21(sp)\n"
        "ld s4,  8 * 22(sp)\n"
        "ld s5,  8 * 23(sp)\n"
        "ld s6,  8 * 24(sp)\n"
        "ld s7,  8 * 25(sp)\n"
        "ld s8,  8 * 26(sp)\n"
        "ld s9,  8 * 27(sp)\n"
        "ld s10, 8 * 28(sp)\n"
        "ld s11, 8 * 29(sp)\n"
        "ld sp,  8 * 30(sp)\n"
        "sret\n"
    );
}

void handle_syscall(trap_frame_t *f) {
    switch (f->a3) {
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            while (1) {
                long ch = getchar();
                if (ch != -1) {
                    f->a0 = ch;
                    break;
                }
                yield();
            }
            break;
        case SYS_EXIT:
            printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        default:
            PANIC("unexpected syscall a3=%llx\n", (uint64_t) f->a3);
    }
}

/* `code` is the lower bits of scause (SCAUSE_INTERRUPT already masked off),
 * i.e. the cause code, NOT the sie/sip bit mask. STIE=32 (mask), but the
 * matching cause code is 5 -- those are different numbers. */
void handle_interrupt(uint64_t code) {
    switch (code) {
        case IRQ_S_TIMER:
            /* Rearm before yield: sip.STIP only clears when the next deadline
             * is programmed, so without this the same interrupt would fire
             * again immediately after sret. */
            sbi_set_timer(read_time() + TIMER_INTERVAL);
            yield();
            break;
        default:
            PANIC("unexpected interrupt code=%llx\n", (uint64_t) code);
    }
}

void handle_trap(trap_frame_t *f) {
    uint64_t scause = READ_CSR(scause);
    uint64_t stval = READ_CSR(stval);
    uint64_t user_pc = READ_CSR(sepc);

    if (scause & SCAUSE_INTERRUPT) {
        /* Async trap: resume the interrupted instruction, do NOT advance
         * sepc the way ecall does. */
        handle_interrupt(scause & SCAUSE_CODE_MASK);
    } else if (scause == EXC_U_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } 
    else {
        PANIC("unexpected trap scause=%llx, stval=%llx, sepc=%llx\n",
              (uint64_t) scause, (uint64_t) stval, (uint64_t) user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}
