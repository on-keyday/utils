/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/user.h>
#include <stdio.h>
#include <stdlib.h>

EXTERN_C void main(void) {
    printf("Hello, world from user!\n");
    char buf[10];
    snprintf(buf, sizeof(buf), "Hello, world from user!\n");
    printf("%s", buf);
    printf("> ");
    int val = getchar();
    putchar(val);
    void* locs[10];
    int loc_i = 0;
    for (int i = 0; i < 300; i++) {
        char* m = (char*)malloc(10);
        char* n = (char*)malloc(20);
        char* o = (char*)malloc(30);
        locs[loc_i++] = o;
        printf("malloc: %p\n", m);
        printf("malloc: %p\n", n);
        printf("malloc: %p\n", o);
        printf("free: %p\n", m);
        free(m);
        printf("free: %p\n", n);
        free(n);
        if (loc_i == 10) {
            for (int j = 0; j < 10; j++) {
                printf("free bulk: %p\n", locs[j]);
                free(locs[j]);
            }
            loc_i = 0;
        }
    }
    size_t allocated = 0;
    for (size_t i = 0;; i++) {
#define DATA_SIZE (i << 2)
        void* oom = malloc(DATA_SIZE);
        printf("malloc: %p\n", oom);
        if (oom) {
            allocated += DATA_SIZE;
        }
        if (!oom) {
            int percent = allocated * 100 / FUTILS_FREESTD_HEAP_MEMORY_SIZE;
            printf("Out of memory at %d (%d/%d byte allocated, %d%%)\n", i, allocated, FUTILS_FREESTD_HEAP_MEMORY_SIZE, percent);
            break;
        }
        if (i % 2 == 0) {
            free(oom);
            allocated -= DATA_SIZE;
        }
    }
}
