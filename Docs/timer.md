# Preemptive Scheduling

This document records the conversion from cooperative (`yield()`-only)
scheduling to timer-driven preemptive scheduling on RISC-V S-mode.

## Goal

Run user processes in time slices without requiring them to voluntarily call
`yield()`. The kernel arms a periodic supervisor-timer interrupt; on each
tick the trap handler invokes the existing scheduler.

## Configuration constants

| Name             | Value           | Where                | Notes                              |
|------------------|-----------------|----------------------|------------------------------------|
| `TIMER_INTERVAL` | `100000` ticks  | `kernel/time.h`      | 10 ms at QEMU `virt`'s 10 MHz mtime |
| `SIE_STIE`       | `1 << 5`        | `kernel/exception.h` | sie/sip bit for S-mode timer       |
| `SSTATUS_SIE`    | `1 << 1`        | `kernel/exception.h` | global S-mode IE (kernel preemption)|
| `SSTATUS_SPP`    | `1 << 8`        | `kernel/exception.h` | previous privilege bit             |
| `SSTATUS_SPIE`   | `1 << 5`        | `kernel/process.h`   | already existed                    |
| `SCAUSE_INTERRUPT`| `1ULL << 63`   | `kernel/exception.h` | scause MSB = async marker          |
| `SCAUSE_S_TIMER` | `5`             | `kernel/exception.h` | scause cause code for STI          |

## Phase 1 — SBI timer wrapper and time read

Files: `kernel/sbi.c`, `kernel/sbi.h`, `kernel/time.c`, `kernel/time.h`,
`run.sh`.

- Added `sbi_set_timer(uint64_t stime_value)` — Legacy SBI v0.1 call (EID
  `0x00`). On RV64 the full 64-bit deadline is passed in `a0`. Firmware
  programs the next timer interrupt and clears any pending `sip.STIP`.
- Added `read_time()` returning the `time` CSR via `rdtime`.
- Added `TIMER_INTERVAL = 100_000` (10 ms slice on a 10 MHz timebase).
- Added `kernel/time.c` to the kernel link line in `run.sh`.

These functions exist but are not yet called, so runtime behavior is
unchanged after Phase 1.

## Phase 2 — Trap dispatch: interrupts vs. exceptions

Files: `kernel/exception.h`, `kernel/exception.c`.

`scause` on RV64 sets bit 63 for asynchronous interrupts; the lower bits are
the cause code. Before this phase `handle_trap()` only recognized `ECALL`
and panicked on anything else.

- Introduced `SCAUSE_INTERRUPT`, `SCAUSE_CODE_MASK`, `SCAUSE_S_TIMER`.
- Split `handle_trap()` into three branches: interrupt, ecall, fault. The
  interrupt path calls a new `handle_interrupt(code)` and **does not advance
  `sepc`** (async traps must resume the interrupted instruction; only ecall
  uses the `sepc + 4` skip).

`handle_interrupt()` started as a stub that panicked; Phase 4 fills in the
timer case.

## Phase 3 — Enable the supervisor timer interrupt source

Files: `kernel/exception.h`, `kernel/kernel.c`.

- Added `SET_CSR` / `CLEAR_CSR` macros (`csrs` / `csrc`) to set or clear bits
  atomically without read-modify-write.
- Defined `SIE_STIE`.
- In `kernel_main`, right after `stvec = kernel_entry`, do `SET_CSR(sie,
  SIE_STIE)`.

The kernel itself still keeps `sstatus.SIE = 0` at this point; user mode
inherits `SIE = 1` from `user_entry()`'s pre-`sret` `sstatus.SPIE = 1` write
via the SPIE→SIE flip on `sret`. Because no timer is armed yet, this phase
also leaves runtime behavior unchanged.

## Phase 4 — Timer handler and first arm

Files: `kernel/exception.c`, `kernel/kernel.c`.

- `handle_interrupt()` now matches `SCAUSE_S_TIMER` (cause code `5`, not the
  bit-mask `SIE_STIE = 32`), rearms the next deadline with
  `sbi_set_timer(read_time() + TIMER_INTERVAL)`, then calls `yield()`.
  Without rearming, `sip.STIP` would remain set and the same interrupt would
  fire immediately after `sret`.
- In `kernel_main`, after `SET_CSR(sie, SIE_STIE)`, arm the first deadline
  with `sbi_set_timer(read_time() + TIMER_INTERVAL)`. Without this very
  first arm, the timer would never fire and preemption would never start.

After Phase 4, U-mode code is preempted on each tick. Kernel-mode code is
still not preempted because `sstatus.SIE` is `0` while the kernel runs.

## Phase 5 — `sscratch` convention for two-mode trap entry

Files: `kernel/exception.c`, `kernel/process.c`, `kernel/kernel.c`,
`kernel/exception.h`.

This phase is required for kernel preemption (Phase 7) to be safe.

### Why the old scheme breaks under kernel preemption

The original `kernel_entry` always swapped `sp` with `sscratch`, assuming
`sscratch` held the current proc's kernel-stack top. When a trap arrives
from S-mode the kernel is already running on its own stack at some
`sp = X`. The old swap put `sp = kernel_top` and reserved a 256-byte trap
frame at `[kernel_top - 256, kernel_top)`. But the kernel had been using the
top of that same stack — placing the trap frame there **overwrites in-use
data**.

### New invariant

```
sscratch == 0           when running in S-mode
sscratch == kernel_top  when running in U-mode
```

### `kernel_entry` (both directions)

```
csrrw sp, sscratch, sp
bnez  sp, .from_user           # sp == 0 iff sscratch was 0 (S-mode entry)
csrr  sp, sscratch             # restore kernel sp (sscratch still = kernel_sp)
.from_user:
addi  sp, sp, -256             # reserve trap frame BELOW current sp
... save 30 GPRs ...
csrr  a0, sscratch             # = user_sp (U entry) or kernel_sp (S entry)
sd    a0, 30*8(sp)             # slot 30 = trapping sp
csrw  sscratch, zero           # mark "now in S-mode" while handler runs
mv    a0, sp; call handle_trap

# On the way out, decide sscratch from sstatus.SPP:
csrr  t0, sstatus
andi  t0, t0, 0x100            # SPP
bnez  t0, .ret_kernel
addi  t0, sp, 256              # = kernel_top for U-mode return
csrw  sscratch, t0
.ret_kernel:                   # SPP=1 -> leave sscratch = 0
... restore GPRs and sp ...
sret
```

For S-mode entry the trap frame lives below the existing kernel stack
contents, which leaves the kernel's in-use frames intact — exactly what
makes kernel preemption safe.

### `yield()`

Removed the explicit `csrw sscratch, ...`. `sscratch` is now managed only
by the trap entry/exit logic plus `user_entry()`.

### `user_entry()`

Dropped `NAKED`. The function now reads `current_proc->stack[]` and
programs `sscratch = kernel_top` before its `sret`. The compiler-emitted
prologue uses a few bytes of the proc's kernel stack, which is harmless
because `sret` transfers control out and any subsequent re-entry rebuilds
`sp` from `sscratch`.

### Boot

`kernel_main` now does `WRITE_CSR(sscratch, 0)` immediately after setting
`stvec`, so any trap that fires before the first `yield()` lands on the
S-mode entry path.

## Phase 6 — Verification

The test bench is QEMU `virt` driven by `run.sh`. Verified non-interactively
with a 4–5 s `timeout` and stdout captured to a file.

Confirmed:

- Boot reaches "`Yielding to shell (user-mode)...`" and the shell prints its
  `> ` prompt — i.e. the first `yield()` correctly switches into U-mode and
  the trap path survives multiple timer ticks.
- No `PANIC` during the 4 s window. With timer firing every 10 ms that is
  ~400 timer interrupts handled, all routed through `handle_interrupt() ->
  yield()`.
- Shell consumes stdin (echoed `xit` from a piped `exit` line), confirming
  the `SYS_GETCHAR` polling loop still yields and resumes correctly under
  preemption.

Not yet exercised:

- **Multi-runnable preemption.** Only the shell exists, so each timer tick
  finds shell as the sole non-idle runnable proc and `yield()` returns
  early. To see actual context switches, add a second user process or a
  CPU-bound kernel thread (`create_kernel_thread`).
- **Kernel preemption under load.** The kernel currently spends almost all
  its S-mode time inside trap handlers (where hardware forces `SIE = 0`),
  so Phase 7's kernel-mode preemption path is reachable mainly via the
  post-shell-exit `wfi` loop and the brief boot window between
  `SET_CSR(sstatus, SSTATUS_SIE)` and the first `yield()`. A kernel
  CPU-burner would make this directly observable.

Suggested follow-up tests:

```c
// Add to kernel_main before yield() for a visible Phase 7 demo:
static void burner(void) {
    for (uint64_t i = 0; ; i++)
        if ((i & 0xFFFFF) == 0) printf(".");
}
create_kernel_thread(burner);
```

If preemption is working, dots and the shell prompt interleave; if Phase 7
is broken, the burner monopolizes the CPU.

## Phase 7 — Kernel preemption

Files: `kernel/kernel.c`, `kernel/process.c`.

Two changes:

1. **Enable `sstatus.SIE` in the kernel.** In `kernel_main`, just before the
   first `yield()`, do `SET_CSR(sstatus, SSTATUS_SIE)`. From this point on
   the kernel runs preemptible: a timer interrupt during S-mode (e.g.
   inside the post-shell `wfi` loop or a future kernel thread) will trap
   into `kernel_entry` via the S-mode path established by Phase 5.

2. **Make `yield()` an interrupt-safe critical section.** The new pattern:

   ```c
   uint64_t prev_sie = READ_CSR(sstatus) & SSTATUS_SIE;
   CLEAR_CSR(sstatus, SSTATUS_SIE);
   ...                                   // scheduler / current_proc / satp
   switch_context(&prev->sp, &next->sp);
   if (prev_sie) SET_CSR(sstatus, SSTATUS_SIE);
   ```

   `prev_sie` is a stack local on the **caller's** kernel stack, so it is
   preserved across arbitrarily many round-trips through other procs. When
   `yield()` was entered from a trap handler `prev_sie` is `0` (hardware
   pre-cleared `SIE` on trap entry); on exit we leave `SIE` cleared and let
   the trap epilogue's `sret` restore it from `SPIE`.

The hardware property that makes nested timer traps impossible during
handling: on trap entry the CPU stores the previous `SIE` into `SPIE` and
forces `SIE = 0`. Since the handler never re-enables `SIE`, no nested
S-mode trap can fire while we are inside `handle_interrupt()` /
`handle_syscall()`. `sret` flips `SIE := SPIE` on the way out.

### Single-hart concurrency model

There are no spinlocks. With one hart the only source of "concurrency" is
preemption, and disabling `SIE` is a sufficient mutual-exclusion mechanism
for short critical sections. The only critical section in this codebase is
the scheduler/`current_proc`/`satp` update inside `yield()`. printf, SBI
calls, and trap handlers are not re-entered by themselves.

### Sequence diagrams

User-mode preemption (the hot path):

```
U: shell running, sstatus.SIE=1
   timer fires
S: kernel_entry  (csrrw sp,sscratch,sp -> sp = shell_kernel_top)
                 (sscratch <- 0 after trap frame saved)
   handle_trap -> handle_interrupt
   sbi_set_timer(now + interval)
   yield()
     prev_sie = 0 (hw cleared SIE)
     scheduler -> next
     csrw satp; sfence; switch_context
S: (next proc's trap-handler context, if it was preempted)
   ... eventually ...
   sret epilogue (sscratch <- next->kernel_top, since SPP=0)
U: next proc resumes, SIE=1 (restored from SPIE)
```

Kernel-mode preemption (post-shell wfi loop):

```
S: wfi loop, sstatus.SIE=1
   timer fires
S: kernel_entry  (sscratch=0 -> S-mode entry path)
                 (sp = kernel_sp; trap frame BELOW that)
   handle_trap -> handle_interrupt -> yield()
   ...
   sret with SPP=1 -> sscratch stays 0, mode stays S, SIE restored from SPIE=1
S: back in wfi loop
```

## Files touched

```
common/common.h         (offsetof macro - unrelated, kept)
kernel/exception.h      Phase 2/3/5 macros
kernel/exception.c      Phase 2/4/5: trap dispatch, handle_interrupt, kernel_entry rewrite
kernel/kernel.c         Phase 3/4/5/7: sie.STIE, first sbi_set_timer, sscratch=0, kernel SIE
kernel/process.c        Phase 5/7: yield (sscratch removed, SIE guard), user_entry rewrite
kernel/sbi.c            Phase 1: sbi_set_timer
kernel/sbi.h            Phase 1: declaration
kernel/time.c           Phase 1: read_time
kernel/time.h           Phase 1: TIMER_INTERVAL, declaration
run.sh                  Phase 1: build kernel/time.c
```

## Things deliberately not done

- **Locking primitives.** Single hart + interrupt-disable is enough for
  this codebase. Adding spinlocks would only matter on SMP.
- **Saving `sepc`/`sstatus` into the trap frame.** The kernel never
  re-enables `SIE` inside a handler, so no nested trap can clobber those
  CSRs. They are read at the top of `handle_trap()` and written back at
  the bottom; that is sufficient.
- **Per-CPU state and TLS.** Single hart only.
- **Priority / fair scheduling.** Round-robin via the existing
  `scheduler()` is left as-is.
- **A demo CPU-burner kernel thread.** Easy to add (see Phase 6 snippet)
  but kept out of the production kernel.
