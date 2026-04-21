#!/bin/bash
set -xue

QEMU=qemu-system-riscv32

# clang 경로와 컴파일 옵션
CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"
INCLUDES="-Icommon -Ikernel -Iuser"

# llvm-objcopy 경로
OBJCOPY=/usr/bin/llvm-objcopy

# Build user program (user/user.c provides its own putchar)
$CC $CFLAGS $INCLUDES -Wl,-Tuser/user.ld -Wl,-Map=shell.map -o shell.elf \
    user/shell.c user/user.c common/common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o

# Build kernel (kernel/sbi.c provides putchar via SBI Console Putchar)
$CC $CFLAGS $INCLUDES -Wl,-Tkernel/kernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel/kernel.c kernel/exception.c kernel/memory.c kernel/process.c \
    kernel/sbi.c kernel/virtio.c kernel/boot.S \
    common/common.c shell.bin.o

# QEMU 실행
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -d unimp,guest_errors,int,cpu_reset -D qemu.log \
    -drive id=drive0,file=lorem.txt,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel kernel.elf
