/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <binary/flags.h>
#include <low/kernel/kernel.h>
#include <freestd/stdio.h>

struct vaddr_f_t {
    futils::binary::flags_t<vaddr_t, 10, 10, 12> flags;
    bits_flag_alias_method(flags, 0, vpn1);
    bits_flag_alias_method(flags, 1, vpn0);
    bits_flag_alias_method(flags, 2, offset);
};

void map_page(uint32_t* table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
    if (__builtin_is_aligned((void*)vaddr, PAGE_SIZE) == false) {
        PANIC("vaddr is not aligned");
    }
    if (__builtin_is_aligned((void*)paddr, PAGE_SIZE) == false) {
        PANIC("paddr is not aligned");
    }
    auto addr = vaddr_f_t(vaddr);
    auto vpn1 = addr.vpn1();
    if ((table1[vpn1] & PAGE_V) == 0) {
        paddr_t pt_paddr = page_alloc(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }
    auto vpn0 = addr.vpn0();
    auto table0 = (paddr_t*)((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}
