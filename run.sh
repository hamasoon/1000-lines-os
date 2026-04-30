#!/bin/bash
set -xue

# if disk.img doesn't exist, create a 512 MiB one filled with zeros
if [ ! -f disk.img ]; then
    dd if=/dev/zero of=disk.img bs=1M count=512
fi

QEMU=qemu-system-riscv64

CC=clang-18
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra \
    --target=riscv64-unknown-elf \
    -march=rv64imac_zicsr_zifencei -mabi=lp64 \
    -mcmodel=medany -mno-relax \
    -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"
INCLUDES="-Icommon -Ikernel -Iuser"

OBJCOPY=/usr/bin/llvm-objcopy

# User program (shell) -> flat binary -> wrapped object, embedded into the kernel ELF.
$CC $CFLAGS $INCLUDES -Wl,-Tuser/user.ld -Wl,-Map=shell.map -o shell.elf \
    user/shell.c user/user.c common/common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -Ibinary -Oelf64-littleriscv shell.bin shell.bin.o

$CC $CFLAGS $INCLUDES -Wl,-Tkernel/kernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel/kernel.c kernel/exception.c kernel/memory.c kernel/process.c \
    kernel/sbi.c kernel/virtio.c kernel/file.c kernel/time.c kernel/boot.S \
    kernel/spinlock.c \
    common/common.c shell.bin.o

# OpenSBI fw_dynamic -> S-mode -> kernel entry @ 0x80200000
$QEMU -machine virt -m 512M -bios default -nographic -serial mon:stdio --no-reboot \
    -d unimp,guest_errors,int,cpu_reset -D qemu.log \
    -drive id=drive0,file=disk.img,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel kernel.elf
