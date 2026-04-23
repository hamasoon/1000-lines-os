# 1000-lines-os

riscv64 타겟의 작은 커널. "Writing an OS in 1,000 Lines" 튜토리얼을 끝낸 뒤
그 위에 xv6-riscv와 Linux를 참고하며 확장해가는 학습/실험용 프로젝트.

- **아키텍처**: RV64IMAC_Zicsr_Zifencei, lp64 ABI, mcmodel=medany
- **가상 메모리**: Sv39 (3단계 페이지 테이블, 9-bit index/level)
- **부팅**: OpenSBI fw_dynamic → S-mode 진입 @ `0x80200000`
- **에뮬레이션**: QEMU `virt` 머신, OpenSBI 기본 펌웨어

## 빌드 & 실행

```bash
bash run.sh
```

`run.sh`는 user(shell) / kernel을 차례로 빌드하고 QEMU로 부팅합니다.
의존성: `clang` (18+), `lld`, `llvm-objcopy`, `qemu-system-riscv64`, `opensbi` (riscv64).

Ubuntu 24.04 기준:
```bash
sudo apt install clang lld llvm qemu-system-misc opensbi
```

## 디렉터리

```
common/      공용 유틸 (memset/memcpy/strcpy/strcmp/printf, 타입 선언)
kernel/      S-mode 커널
  boot.S       OpenSBI → S-mode 진입점, 보조 hart park
  kernel.c     kernel_main: 초기화 → paging → virtio → shell 실행
  memory.c/h   Sv39 페이지 테이블, map_page, enable_paging
  process.c/h  프로세스/커널 스레드/스케줄러/switch_context/yield
  exception.c/h  트랩 벡터 (kernel_entry), handle_syscall, handle_trap
  sbi.c/h      SBI Console Putchar/Getchar wrapper
  virtio.c/h   virtio-blk (legacy MMIO) 드라이버
  kernel.ld    링커 스크립트 (kernel base = 0x80200000)
user/        U-mode 프로그램 (shell)
  shell.c      간단한 echo/exit 셸
  user.c/h     syscall wrapper (ecall 경유)
  user.ld      링커 스크립트 (USER_BASE = 0x1000000)
Docs/        내부 노트 (boot, panic, SBI 호출 등)
```

## 현재 부팅 플로우

```
OpenSBI (M-mode)
 └─ boot.S: S-mode 진입, a0=hartid, a1=FDT
    └─ kernel_main
       ├─ init_memory         bss 0-fill, free_ram 시작점 고정
       ├─ stvec = kernel_entry
       ├─ enable_paging       Sv39 root PT + 커널/virtio identity map → satp
       ├─ virtio_init         MMIO reset → queue 0 설정
       ├─ read_disk(sec 0)    "Lorem ipsum..." 확인 출력
       ├─ write_disk(sec 0)   "hello from riscv64 kernel!"
       ├─ create_process(NULL)       idle = boot ctx
       ├─ create_process(shell_bin)  유저 프로세스
       └─ yield
          ├─ switch_context → user_entry → sret (SPP=0)
          └─ shell U-mode 실행
             ├─ printf("> ") → putchar → ecall → SBI
             ├─ getchar loop (ecall)
             └─ "exit" → SYS_EXIT → EXITED → yield
       └─ (idle 복귀) "Shell exited" → wfi
```

## 시스템 콜

| # | 이름         | 역할                                    |
|---|--------------|-----------------------------------------|
| 1 | SYS_PUTCHAR  | 문자 출력 (SBI Console Putchar로 위임)  |
| 2 | SYS_GETCHAR  | 문자 입력 (SBI Console Getchar busy-wait) |
| 3 | SYS_EXIT     | 프로세스 종료 (EXITED → yield)          |
| 4 | SYS_READ     | (예약) 파일 읽기 — 아직 미구현          |
| 5 | SYS_WRITE    | (예약) 파일 쓰기 — 아직 미구현          |

## 메모리 레이아웃

```
0x00000000_10001000  VIRTIO_BLK MMIO (legacy)
0x00000000_01000000  USER_BASE (유저 프로세스 이미지)
0x00000000_80000000  OpenSBI
0x00000000_80200000  커널 (__kernel_base)
  ├─ .text / .rodata / .data / .bss
  ├─ 128KB boot 스택 → __stack_top
  └─ __free_ram (4KB 정렬)
     ├─ 페이지 테이블, 프로세스별 자원
     └─ __free_ram_end = __free_ram + 64MB
```

## 레퍼런스

- [Writing an OS in 1,000 Lines](https://operating-system-in-1000-lines.vercel.app/)
- [xv6-riscv](https://github.com/mit-pdos/xv6-riscv) — 특히 `kernel/vm.c`, `kernel/riscv.h`, `kernel/trap.c`
- [RISC-V Privileged Spec](https://riscv.org/specifications/privileged-isa/)
- [OpenSBI](https://github.com/riscv-software-src/opensbi)
- [Virtio 1.0 Spec](https://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html) (legacy MMIO)

---

# TODO

자체 OS로 확장해가며 다룰 과제 목록. 우선순위 ≒ 위쪽이 먼저.

## 🧹 위생 (Hygiene)

- [ ] **`sscratch` 초기화 타이밍 정리**
  현재 첫 `yield()` 전까지 `sscratch`가 undefined. 부팅 초기 `WRITE_CSR(stvec, ...)`
  직후 `sscratch = __stack_top`으로 세팅해 "트랩이 언제 발생해도 안전"한 상태로.
- [ ] **커널 vs 프로세스 부트 스택 분리**
  지금은 idle = boot ctx라 `__stack_top`(kernel.ld 128KB 스택)이 그대로 idle의
  스택으로 재사용됨. 소유권이 모호해 디버깅 어려움. boot 스택은 단발성으로 쓰고
  idle은 자체 스택을 갖게.
- [ ] **페이지 권한 세분화**
  `enable_paging`/`map_kernel`이 커널 전 영역을 `R|W|X`로 매핑. `.text`는 `R|X`,
  `.rodata`는 `R`, `.data/.bss/stack`은 `R|W`로 쪼개 write-exec 방지.
- [ ] **`common/printf`에 `%zd`/`%zu`, 폭/정밀도 지정**
  `size_t` 출력 시 매번 `(unsigned long)` 캐스트 필요 — 번거롭고 이식성 낮음.

## 🏗️ 구조 리팩토링

- [ ] **커널 VA 상위 반으로 (high-half kernel)**
  현재 모든 프로세스 PT에 `map_kernel`을 34장 PT 복제로 수행. Linux/xv6 스타일로
  상위 VA를 커널 전용으로 삼고 한 번만 구성한 뒤 프로세스 PT가 공유하면 PT
  사용량이 ~34× 감소하고 kernel↔user 진입 시 TLB flush 범위도 명확해짐.
- [ ] **ELF 로더**
  지금은 `objcopy -O binary`의 flat binary를 페이지 단위 복사. ELF를 읽어
  PT_LOAD 세그먼트별로 권한을 정확히 매핑하도록. 정적 실행파일 지원 먼저,
  추후 동적 섹션 확장.
- [ ] **메모리 할당기**
  `alloc_pages`는 bump allocator. 해제 불가. slab/buddy 기반 페이지 할당기 +
  커널 객체용 kmalloc 레이어 (xv6의 `kmem.freelist` 수준이면 시작으로 충분).

## 🔀 동시성 / 스케줄링

- [ ] **타이머 인터럽트 + preemptive scheduler**
  OpenSBI 로그상 `sstc`(S-mode timer) 확장 지원. `stimecmp` 설정으로 주기적
  S-mode 타이머 인터럽트 → `yield`로 타임슬라이스 구현.
- [ ] **멀티 hart (SMP) 대응**
  현재 `boot.S`는 hart ≠ 0을 wfi park. HSM (Hart State Management) SBI 확장으로
  보조 hart 기상. per-CPU 구조체 (`tp` 레지스터에 per-CPU 포인터 저장 관례),
  spinlock, 스케줄러 큐 locking.
- [ ] **IPI (Inter-Processor Interrupt)**
  SBI IPI/Extension으로 TLB shootdown, remote wake 구현.

## 🧩 인터럽트 / I/O

- [ ] **PLIC 초기화 + 외부 인터럽트 처리**
  현재는 interrupt-driven I/O 없음 (virtio도 busy-wait). PLIC → virtio 인터럽트 →
  waiter wake-up 구조로. QEMU virt의 PLIC 베이스는 `0x0C000000`.
- [ ] **virtio-blk 비동기 + 드라이버 캐시**
  현재 `__read_write_disk`는 busy-wait (`while (virtq_is_busy)`). 인터럽트 기반
  완료 통지로 프로세스를 재울 수 있게.
- [ ] **UART 직결 드라이버 (SBI 탈피)**
  현재 putchar/getchar는 SBI ecall. 학습용으로 NS16550A UART (QEMU virt
  `0x10000000`) 직접 제어.

## 🗄️ 파일시스템

- [ ] **tarfs (읽기 전용)**
  레포에 `kernel/file.c/h` 스켈레톤 존재. USTAR 포맷 ramdisk를 shell.bin과
  같은 방식으로 임베드하고 `read()` syscall로 읽기.
- [ ] **블록 캐시**
  sector 단위 캐시 → FS 레이어. xv6 `bio.c` 참고.
- [ ] **간단한 inode/디렉토리 FS**
  v6-style file/inode (xv6의 `fs.c` 포팅)이 쓸만한 중간 목표.

## 👤 사용자 공간

- [ ] **syscall ABI 확장**
  `fork`, `exec`, `wait`, `pipe`, `open`, `close`, `kill`, `sbrk` — xv6 ABI를
  단계적으로.
- [ ] **여러 유저 프로그램**
  지금은 shell 하나만 임베드. initrd 또는 임베드 다중 이미지 지원 후 `exec`으로
  교체 가능하게.
- [ ] **signal / `kill`**
  프로세스 종료 외 async 통지 메커니즘.

## 🔎 관측/디버깅

- [ ] **panic 스택 트레이스**
  `PANIC` 시 현재 `s0(fp)` 체인을 따라 콜스택 출력.
- [ ] **`assert` 매크로 + 컴파일 타임 체크**
  `_Static_assert`로 트랩 프레임 크기 256B, `process_t.stack` 16정렬 등
  불변식 고정.
- [ ] **GDB stub / QEMU gdbserver 가이드 `Docs/`에**
  `qemu -s -S` → `riscv64-elf-gdb kernel.elf` 워크플로우 정리.

## 🐞 알려진 이슈

- [ ] QEMU `-serial mon:stdio` 파이프 입력 시 첫 문자가 종종 유실
  (rv64 포팅과 무관, serial 모드 변경 또는 shell 측 drain 로직 검토)
- [ ] `common/printf %x` 기본이 8자리 고정 — 의도한 "짧은 int 16진"과
  "상위 0 생략"의 선호가 혼재. 정책 결정 후 일괄 수정 필요
