/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/string.h>
#include <freestd/stdio.h>
#include <low/kernel/kernel.h>

extern char __bss[], __bss_end[], __stack_top[];

// clang-format off
__attribute__((section(".text.boot"))) __attribute__((naked))
EXTERN_C void boot(void) {
// clang-format on
#if __X86__
    __asm__ volatile(
        "mov sp,%0\n"
        "jmp kernel_main\n"
        : : "r"(__stack_top));
#elif __riscv
    __asm__ volatile(
        "mv sp,%0\n"
        "j kernel_main\n"
        : : "r"(__stack_top));
#endif
}

void proc_a_entry(void) {
    printf("start process A\n");
    while (1) {
        printf("A\n");
        yield();

        for (int i = 0; i < 10000000; i++) {
            __asm__ volatile("nop");
        }
    }
}

void proc_b_entry(void) {
    printf("start process B\n");
    while (1) {
        printf("B\n");
        yield();

        for (int i = 0; i < 10000000; i++) {
            __asm__ volatile("nop");
        }
    }
}

extern char _binary___built_kernel_shell_bin_start[];
extern char _binary___built_kernel_shell_bin_size[];

EXTERN_C void kernel_main(void) {
    memset(__bss, 0, __bss_end - __bss);
    WRITE_CSR(stvec, (uint32_t)kernel_exception_entry);
    process_init();
    // struct process* a = create_kernel_process(proc_a_entry);
    // struct process* b = create_kernel_process(proc_b_entry);

    struct process* u = create_user_process(_binary___built_kernel_shell_bin_start, (size_t)_binary___built_kernel_shell_bin_size);

    yield();
    PANIC("switched to idle process\n");
}
