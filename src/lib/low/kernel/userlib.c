/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <freestd/stdlib.h>
#include <freestd/stdio.h>
#include <low/kernel/user.h>

extern char __stack_top[];

EXTERN_C int syscall(int sys, int arg0, int arg1, int arg2) {
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sys;

    __asm__ volatile("ecall"
                     : "=r"(a0)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                     : "memory");

    return a0;
}

EXTERN_C int putchar(int c) {
    return syscall(SYS_putchar, c, 0, 0);
}

EXTERN_C int getchar() {
    return getchar_nonblock(0);
}

EXTERN_C int getchar_nonblock(int retry) {
    return syscall(SYS_getchar, retry, 0, 0);
}

__attribute__((noreturn)) void exit(int status) {
    syscall(SYS_exit, status, 0, 0);
    while (1) {
        __asm__ volatile("unimp");  // this may cause trap
    }
}

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((section(".text.start")))
__attribute__((naked)) void
start() {
    __asm__ volatile(
        "mv sp, %0\n"
        "call main\n"
        "mv a0, zero\n"
        "call exit\n" : : "r"(__stack_top));
}

#ifdef __cplusplus
}
#endif
