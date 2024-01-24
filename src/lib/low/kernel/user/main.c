/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/user.h>
#include <stdio.h>

EXTERN_C void main(void) {
    printf("Hello, world from user!\n");
    printf("Press any key to continue...\n");
    getchar();
}
