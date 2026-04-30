# Multi-hart (SMP) 대응 계획

riscv64 멀티 hart 지원으로 가는 단계별 계획. xv6-riscv를 기본 참조로,
`cpu_t`/`tp`/spinlock 도입 → sscratch 재설계 → HSM으로 보조 hart 기상 →
IPI/TLB shootdown 순서로 단계적 확장.

## 현재까지 (완료)

- ✅ **Phase 0** — spinlock API 확정 (`spinlock_acquire/release`, flags는 caller stack)
- ✅ **Phase 1** — [spinlock.c](kernel/spinlock.c) / [spinlock.h](kernel/spinlock.h) 빌드 통합
- ✅ **Phase 2** — `local_irq_save/restore` (sstatus.SIE 토글, `static inline` in 헤더)
- ✅ **Phase 3** — spinlock 본체 (`amoswap.w.aq` 기반 acquire, fence + 순서 정확한 release, `holding()` 체크)

현재 [spinlock.c](kernel/spinlock.c)는 단일 hart 가정으로 동작. `cpu_t *owner` 슬롯과 `get_cpu()` 호출은 자리만 잡힌 상태 — `mycpu()` 인프라(Phase A)가 깨어나면 의미를 가짐.

---

## Phase 4 — 첫 사용처: `ptable.lock` (단일 hart에서 검증)

목표: [process.c:191-225](kernel/process.c#L191-L225)의 수동 SIE 토글 (`prev_sie` 저장/복원) 을 spinlock으로 흡수. 단일 hart에서도 가치 있음 (코드 단순화, 트랩 컨텍스트 안전성 명시화).

### 4-1. ptable 도입
- `process.c` 상단에 `static struct { spinlock_t lock; } ptable;`
- `kernel_main` 초기화 시 `spinlock_init(&ptable.lock, "ptable")`
- 기존 `static process_t procs[PROCS_MAX]`를 `ptable.procs[]`로 옮길지는 기호. 락과 자료의 인접성을 위해선 옮기는 게 정석

### 4-2. yield() 재작성
```c
void yield(void) {
    uint64_t flags;
    spinlock_acquire(&ptable.lock, &flags);

    process_t *next = scheduler();
    if (next == current_proc) {
        spinlock_release(&ptable.lock, flags);
        return;
    }

    /* satp + sfence + current_proc 갱신 */
    /* switch_context — 다음 thread가 락 들고 깨어남! */
    switch_context(&prev->sp, &next->sp);

    /* 깨어났을 때 — 우리 짝의 release */
    spinlock_release(&ptable.lock, flags);
}
```

기존 [process.c:191-198](kernel/process.c#L191-L198), [process.c:224-225](kernel/process.c#L224-L225) 의 수동 SIE 코드 전부 삭제.

### 4-3. 첫 실행 경로 처리 (xv6 식 `forkret` 패턴)
새로 만들어진 process는 처음 깨어날 때 `switch_context`의 `ret` 직후 `user_entry` (또는 kernel thread entry) 로 점프. 이때 **락을 들고 깨어나므로** 직접 풀어줘야 함.

- [process.c:99 user_entry](kernel/process.c#L99) 첫 줄에:
  ```c
  spinlock_release(&ptable.lock, /*flags=*/SSTATUS_SIE);
  ```
- `create_kernel_thread`로 만든 thread도 entry 첫 줄에 같은 호출 필요 — 또는 공통 wrapper `forkret`를 만들어서 ra로 박아두고 거기서 release한 뒤 진짜 entry로 jump

flags 값은 어떻게 정하나? 첫 깨어남은 항상 "사용자 코드 진입 = SIE on이 자연" 이므로 `SSTATUS_SIE` 하드코딩. xv6도 동일.

### 4-4. 검증
- `bash run.sh` → 셸 부팅, 입출력 정상, exit 정상
- `[time.h](kernel/time.h)` 의 `TIMER_INTERVAL` 을 짧게 줄여서 (예: 10ms → 1ms) timer-driven yield 스트레스
- 의도적 데드락 시도 (recursive acquire) → PANIC 메시지 확인
- 의도적 mismatched release → PANIC 확인

**여기까지 단일 hart에서 완성.** 이후 단계는 SMP 인프라.

---

## Phase A — per-CPU 골격 (`cpu_t`, `tp` 레지스터)

목표: hart 식별과 per-hart 상태 분리. SMP 모든 후속 단계의 토대.

### A-1. cpu_t 정리
- 이미 [process.h:48-52](kernel/process.h#L48-L52)에 정의됨:
  ```c
  typedef struct __cpu {
      struct __process *proc;
      context_t context;
      uint64_t hart_id;
  } cpu_t;
  ```
- `noff` / `intena` 같은 깊이 카운터는 **불필요** (Linux식 flags-on-stack 채택했으므로)
- `static cpu_t cpus[MAX_CPUS]` 배열 도입

### A-2. tp 레지스터에 per-CPU 포인터
- [boot.S](kernel/boot.S) 수정: hart별로 `tp = &cpus[a0]` 세팅 후 진입
- `mycpu()` 인라인:
  ```c
  static inline cpu_t *mycpu(void) {
      cpu_t *c;
      __asm__ __volatile__("mv %0, tp" : "=r"(c));
      return c;
  }
  ```
- `get_cpu()`를 `mycpu()`로 리네임하거나 wrapper로 통일

### A-3. tp의 트랩 보존
- [exception.c kernel_entry](kernel/exception.c#L23) 에서 이미 `tp`를 trap frame slot 2에 저장/복원 중 ✓
- **유저 진입 시 `tp` 재로드**: 유저가 `tp`를 더럽힐 수 있으므로, U→S 트랩 진입 직후 `tp`를 `sscratch` 기반으로 재구성 필요. xv6는 trapframe 구조체에 hartid를 박아두고 거기서 로드. 우리도 sscratch 규약 재설계(Phase C)와 같이 진행.

### A-4. `current_proc` 전역 제거
- [process.c:10-11](kernel/process.c#L10-L11)의 `current_proc`/`idle_proc` 전역 → `mycpu()->proc`, `mycpu()->idle_proc`
- 모든 사용처 (`process.c`, `exception.c handle_syscall`) 통과
- `idle_proc` 는 hart마다 하나씩 → `cpu_t` 안에 슬롯 추가 또는 `idle_procs[MAX_CPUS]`

### A-5. 검증
- 단일 hart로 부팅 (보조 hart는 여전히 park) — 모든 동작이 이전과 동일해야 함
- `mycpu()->hart_id == 0` 확인
- spinlock의 `holding()`을 `lock->locked && lock->owner == mycpu()`로 강화

---

## Phase C — sscratch 규약 재설계

목표: 현재 [exception.c:7-20](kernel/exception.c#L7-L20)의 `sscratch == 0` (S-mode) / `sscratch == kernel_top` (U-mode 진입) 규약을 hart별로 안전한 형태로.

### 두 갈래 — 결정 필요

**옵션 1 (xv6 방식, 권장)**: `sscratch`에 항상 per-CPU `trapframe*` 포인터를 박아둠.
- 트랩 진입 → `csrrw a0, sscratch, a0` 으로 trapframe 주소 받음
- trapframe에서 kernel sp, tp, satp 로드
- S-mode/U-mode 진입 분기 코드 사라짐 — 항상 동일 경로
- 단점: 기존 `kernel_entry` 거의 전체 재작성

**옵션 2 (증분)**: 현재 규약 유지.
- `sscratch == 0` (S-mode 진입), `sscratch == kernel_top` (U-mode 진입)
- 각 hart의 `kernel_top`은 `mycpu()->proc->stack` 기반이라 자동 분리
- Phase A가 끝나면 추가 변경 거의 없이 동작
- 단점: 나중에 시그널/유저 trapframe을 별도 페이지에 둘 때 결국 옵션 1로 가게 됨

증분 가치를 우선하면 옵션 2로 시작 → ELF 로더 또는 시그널 작업 들어갈 때 옵션 1로 마이그레이션.

### C-1. tp 재로드
- 옵션 1이면 trapframe에 `kernel_tp` 슬롯 두고 그것으로 로드
- 옵션 2면 sscratch가 가리키는 stack 위치에서 hart id 추출 (예: kernel_top 직전 슬롯에 cpu_t* 박아두기)

### C-2. user_entry 수정
- [process.c:99](kernel/process.c#L99) 의 `csrw sscratch, kernel_top` 로직을 옵션에 맞게 갱신
- 옵션 1이면 `csrw sscratch, &mycpu()->trapframe`

### C-3. 검증
- 단일 hart 부팅 → 셸 정상
- 트랩 경로별 (timer / ecall / page fault) 진입/이탈 모두 정상

---

## Phase D — 보조 hart 기상 (HSM SBI)

목표: park된 hart들을 깨워서 같은 커널을 돌리게 함.

### D-1. HSM 호출 wrapper
- [sbi.c](kernel/sbi.c) 에 `sbi_hart_start(hartid, start_addr, opaque)` 추가
- HSM extension ID = 0x48534D, function ID = 0 (HART_START)

### D-2. 보조 hart entry
- [boot.S](kernel/boot.S) 의 `park` 분기 제거하지 말고, **별도 `secondary_boot` 심볼 추가**:
  ```asm
  secondary_boot:
      la sp, secondary_stacks      /* per-hart 스택 — A-1에서 cpus[hartid].stack */
      add sp, sp, a0, lsl #14      /* a0 = hartid, 16KB 스택 */
      tail secondary_main
  ```
- C 측 `secondary_main(uint64_t hartid)`:
  - `tp = &cpus[hartid]`
  - `stvec`, `sscratch`, `satp`, `sie` 세팅 (hart 0와 동일)
  - 자기 idle thread로 jump
- 기존 `boot` 의 park는 유지하되 **kernel_main 끝부분에서 모든 hart를 깨운 뒤** park된 hart는 이제 깨어나서 secondary_main으로 진입

### D-3. kernel_main 끝에서 hart 기상
```c
for (int i = 1; i < MAX_CPUS; i++)
    sbi_hart_start(i, (uint64_t)secondary_boot_phys, 0);
```

### D-4. 추가 락 — 진짜 SMP 경쟁 시작
이 시점부터 **공유 자원 모두 락 필요**. 우선순위:
1. **`kmem.lock`** — [memory.c alloc_pages](kernel/memory.c) bump pointer
2. **`cons.lock`** — `printf` / SBI putchar (출력 깨짐 방지)
3. **virtio queue lock** — virtio_blk request 큐
4. **sfs lock** — file system 자료구조

각 자원에 spinlock 추가 + 사용처 `acquire/release` 감싸기.

### D-5. 검증
- 부팅 로그에 모든 hart의 "Hello from hartN" 확인
- 셸은 hart 0에서만 실행, 다른 hart는 idle (wfi loop)
- 의도적 stress: 여러 hart에서 동시에 `alloc_pages` 호출 → 메모리 일관성

---

## Phase E — IPI / TLB shootdown

목표: hart 간 동기 통신.

### E-1. SBI IPI wrapper
- `sbi_send_ipi(hart_mask, hart_mask_base)` — SBI legacy ext 또는 sPI extension
- 인터럽트 코드 `IRQ_S_SOFT` 핸들러 등록

### E-2. TLB shootdown
- `tlb_shootdown(vaddr)`:
  - per-CPU pending list에 vaddr 삽입
  - `sbi_send_ipi(other_harts)`
  - 보낸 쪽도 자기 sfence
- 받는 쪽 핸들러: pending list 소비 + `sfence.vma vaddr`

### E-3. 사용처
- 공유 PT의 unmap / 권한 강등 (현재 우리는 PT가 process별이라 당장 필요 없음)
- **high-half kernel** ([README.md:123-126](README.md#L123-L126)) 도입 시 필수

### E-4. 검증
- `tlb_shootdown` 호출 → 모든 hart에서 sfence 완료 확인
- 권한 강등 후 보호된 페이지 접근 시 즉시 fault (모든 hart에서)

---

## 결정 보류 항목 (도착 시 결정)

1. **sscratch 옵션 1 vs 2** — Phase C 진입 시점. 시그널 도입 계획이 가까우면 옵션 1로 한 번에.
2. **`procs[]` 배열을 `ptable` 안으로 옮길지** — 코드 미적 선택. 옮기면 락-자료 인접성 ↑
3. **`forkret` wrapper vs entry 첫 줄에 release 추가** — wrapper 방식이 더 깔끔하지만 어셈 stub 한 개 추가
4. **`MAX_CPUS`** — 현재 [process.h:6](kernel/process.h#L6)에 4. QEMU `-smp` 옵션과 일치시킬 것
5. **`secondary_boot` 의 위치** — boot.S vs 별도 .S 파일

## 참조

- [xv6-riscv start.c, main.c, swtch.S, trampoline.S](https://github.com/mit-pdos/xv6-riscv) — 거의 그대로 매핑
- [RISC-V SBI Spec](https://github.com/riscv-non-isa/riscv-sbi-doc) — HSM (§9), IPI (§7), Time (§6)
- [RISC-V Privileged Spec](https://riscv.org/specifications/privileged-isa/) — sscratch, sip/sie 비트
- [README.md](README.md) — 큰 그림 TODO 리스트
