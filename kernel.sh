#!/bin/bash



QEMU=qemu-system-riscv32

CC=clang++
OBJCOPY=llvm-objcopy

CFLAGS="-std=c++20 -O2 -g3 -Wall -Wextra --target=riscv32 -ffreestanding -nostdlib -I./src/include"

OUTPUT_DIR=./built/kernel
OUTPUT=$OUTPUT_DIR/kernel.elf
KERNEL_MAP=$OUTPUT_DIR/kernel.map
SRC_DIR=./src/lib/low/kernel
STD_DIR=./src/lib/freestd
STD_INCLUDE_DIR="-I./src/include/freestd -I./src/include"
KERNEL_C_SRC="$SRC_DIR/entry.c $SRC_DIR/sbi.c"
KERNEL_CPP_SRC="$SRC_DIR/trap.cpp $SRC_DIR/heap.cpp $SRC_DIR/process.cpp $SRC_DIR/page_table.cpp $SRC_DIR/syscall.cpp"
STD_SRC="$STD_DIR/printf.cpp"

KERNEL_LINKER_SCRIPT=./src/lib/low/kernel/kernel.ld
FREESTD_DEFINE="-D__FUTILS_FREESTANDING__"

if [ ! -e $OUTPUT_DIR ]; then
    mkdir -p $OUTPUT_DIR
fi

SHELL_MAP=$OUTPUT_DIR/shell.map
SHELL_ELF=$OUTPUT_DIR/shell.elf
SHELL_BIN=$OUTPUT_DIR/shell.bin
SHELL_BIN_O=$OUTPUT_DIR/shell.bin.o
SHELL_SRC_DIR=./src/lib/low/kernel/user
SHELL_SRC="$SRC_DIR/userlib.c $SHELL_SRC_DIR/main.c"
SHELL_LINKER_SCRIPT=$SHELL_SRC_DIR/user.ld


# build shell

$CC $CFLAGS -Wl,-T$SHELL_LINKER_SCRIPT -Wl,-Map=$SHELL_MAP $STD_INCLUDE_DIR $FREESTD_DEFINE -o $SHELL_ELF $SHELL_SRC  $STD_SRC 
if [ $? -ne 0 ]; then
    echo "Build failed."
    return 1
fi

$OBJCOPY --set-section-flags .bss=alloc,contents -O binary $SHELL_ELF $SHELL_BIN    

if [ $? -ne 0 ]; then
    echo "Build failed."
    return 1
fi

$OBJCOPY -Ibinary -Oelf32-littleriscv $SHELL_BIN $SHELL_BIN_O

if [ $? -ne 0 ]; then
    echo "Build failed."
    return 1
fi


# build kernel
$CC $CFLAGS -Wl,-T$KERNEL_LINKER_SCRIPT -Wl,-Map=$KERNEL_MAP -o $OUTPUT $STD_INCLUDE_DIR $FREESTD_DEFINE $STD_SRC $KERNEL_C_SRC $KERNEL_CPP_SRC $SHELL_BIN_O

if [ $? -ne 0 ]; then
    echo "Build failed."
    return 1
fi

#return 0
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -kernel $OUTPUT
