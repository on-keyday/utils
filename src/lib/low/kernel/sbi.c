/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/kernel.h>
#ifdef __cplusplus
extern "C" {
#endif

void putchar(int c) {
    sbi_call(c, 0, 0, 0, 0, 0, 0, 1);
}

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid) {
#if __riscv
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ volatile("ecall" : "=r"(a0), "=r"(a1) : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7) : "memory");
    struct sbiret ret {
        .error = a0, .value = a1
    };
    return ret;
#endif
}

#ifdef __cplusplus
}
#endif
