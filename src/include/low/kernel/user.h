/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#ifdef __cplusplus
#define EXTERN_C extern "C"
extern "C" {
#else
#define EXTERN_C
#endif

int syscall(int sys, int arg0, int arg1, int arg2);
#define SYS_exit 0
#define SYS_putchar 1
#define SYS_getchar 2

#ifdef __cplusplus
}
#endif