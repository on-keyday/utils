/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
        putchar('A');
        yield();

        for (int i = 0; i < 10000000; i++) {
            __asm__ volatile("nop");
        }
    }
}

void proc_b_entry(void) {
    printf("start process B\n");
    while (1) {
        putchar('B');
        yield();

        for (int i = 0; i < 10000000; i++) {
            __asm__ volatile("nop");
        }
    }
}

EXTERN_C void kernel_main(void) {
    memset(__bss, 0, __bss_end - __bss);
    WRITE_CSR(stvec, (uint32_t)kernel_exception_entry);
    printf("\n\nHello %s\n", "World!");
    printf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);
    paddr_t p = page_alloc(2);
    printf("page_alloc(1) = %p\n", p);
    paddr_t p2 = page_alloc(1);
    printf("page_alloc(2) = %p\n", p2);
    process_init();

    struct process* a = create_process(proc_a_entry);
    struct process* b = create_process(proc_b_entry);

    yield();
    PANIC("switched to idle process\n");
}
