/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/kernel.h>
#define SAVE_REG(reg, index) __asm__ volatile("sw " #reg ", 4 * " #index "(sp)\n")
#define LOAD_REG(reg, index) __asm__ volatile("lw " #reg ", 4 * " #index "(sp)\n")

__attribute__((naked))
__attribute__((aligned(4)))
EXTERN_C void
kernel_exception_entry() {
    __asm__ volatile(
        "csrrw sp, sscratch, sp\n"  // swap kernel stack and user/kernel stack
        "addi sp, sp, -4 * 31");
    SAVE_REG(ra, 0);
    SAVE_REG(gp, 1);
    SAVE_REG(tp, 2);
    SAVE_REG(t0, 3);
    SAVE_REG(t1, 4);
    SAVE_REG(t2, 5);
    SAVE_REG(t3, 6);
    SAVE_REG(t4, 7);
    SAVE_REG(t5, 8);
    SAVE_REG(t6, 9);
    SAVE_REG(a0, 10);
    SAVE_REG(a1, 11);
    SAVE_REG(a2, 12);
    SAVE_REG(a3, 13);
    SAVE_REG(a4, 14);
    SAVE_REG(a5, 15);
    SAVE_REG(a6, 16);
    SAVE_REG(a7, 17);
    SAVE_REG(s0, 18);
    SAVE_REG(s1, 19);
    SAVE_REG(s2, 20);
    SAVE_REG(s3, 21);
    SAVE_REG(s4, 22);
    SAVE_REG(s5, 23);
    SAVE_REG(s6, 24);
    SAVE_REG(s7, 25);
    SAVE_REG(s8, 26);
    SAVE_REG(s9, 27);
    SAVE_REG(s10, 28);
    SAVE_REG(s11, 29);

    __asm__ volatile(
        "csrr a0, sscratch\n");

    SAVE_REG(a0, 30);

    __asm__ volatile(
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n");  // restore kernel/user stack

    __asm__ volatile(
        "mv a0, sp\n"
        "call handle_exception\n");

    LOAD_REG(ra, 0);
    LOAD_REG(gp, 1);
    LOAD_REG(tp, 2);
    LOAD_REG(t0, 3);
    LOAD_REG(t1, 4);
    LOAD_REG(t2, 5);
    LOAD_REG(t3, 6);
    LOAD_REG(t4, 7);
    LOAD_REG(t5, 8);
    LOAD_REG(t6, 9);
    LOAD_REG(a0, 10);
    LOAD_REG(a1, 11);
    LOAD_REG(a2, 12);
    LOAD_REG(a3, 13);
    LOAD_REG(a4, 14);
    LOAD_REG(a5, 15);
    LOAD_REG(a6, 16);
    LOAD_REG(a7, 17);
    LOAD_REG(s0, 18);
    LOAD_REG(s1, 19);
    LOAD_REG(s2, 20);
    LOAD_REG(s3, 21);
    LOAD_REG(s4, 22);
    LOAD_REG(s5, 23);
    LOAD_REG(s6, 24);
    LOAD_REG(s7, 25);
    LOAD_REG(s8, 26);
    LOAD_REG(s9, 27);
    LOAD_REG(s10, 28);
    LOAD_REG(s11, 29);
    LOAD_REG(sp, 30);
    __asm__ volatile("sret\n");
}

EXTERN_C void handle_exception(struct trap_frame* f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        syscall_handler(f);
        user_pc += 4;
    }
    else {
        PANIC("unexpected trap scause=0x%x, stval=0x%x, sepc=0x%x\n", scause, stval, user_pc);
    }
    WRITE_CSR(sepc, user_pc);
}

__attribute__((naked)) EXTERN_C void switch_context(vaddr_t* prev_sp, vaddr_t* next_sp) {
    __asm__ volatile("addi sp, sp, -4 * 13");
    SAVE_REG(ra, 0);
    SAVE_REG(s0, 1);
    SAVE_REG(s1, 2);
    SAVE_REG(s2, 3);
    SAVE_REG(s3, 4);
    SAVE_REG(s4, 5);
    SAVE_REG(s5, 6);
    SAVE_REG(s6, 7);
    SAVE_REG(s7, 8);
    SAVE_REG(s8, 9);
    SAVE_REG(s9, 10);
    SAVE_REG(s10, 11);
    SAVE_REG(s11, 12);
    __asm__ volatile("sw sp, (a0)");  // save prev sp
    __asm__ volatile("lw sp, (a1)");  // load next sp
    LOAD_REG(ra, 0);
    LOAD_REG(s0, 1);
    LOAD_REG(s1, 2);
    LOAD_REG(s2, 3);
    LOAD_REG(s3, 4);
    LOAD_REG(s4, 5);
    LOAD_REG(s5, 6);
    LOAD_REG(s6, 7);
    LOAD_REG(s7, 8);
    LOAD_REG(s8, 9);
    LOAD_REG(s9, 10);
    LOAD_REG(s10, 11);
    LOAD_REG(s11, 12);
    __asm__ volatile("addi sp, sp, 4 * 13");
    __asm__ volatile("ret");
}

EXTERN_C void exit(int) {
    for (;;) {
        __asm__ volatile("wfi");
    }
}
