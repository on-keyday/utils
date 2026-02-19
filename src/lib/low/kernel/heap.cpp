/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <low/kernel/kernel.h>
#include <freestd/string.h>

extern char __free_ram[], __free_ram_end[];

EXTERN_C paddr_t page_alloc(size_t n_page) {
    static paddr_t next = (paddr_t)__free_ram;
    paddr_t ret = next;
    next += n_page * PAGE_SIZE;

    if (next > (paddr_t)__free_ram_end) {
        PANIC("Out of memory");
    }
    memset((void*)ret, 0, n_page * PAGE_SIZE);
    return ret;
}
