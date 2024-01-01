/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <wrap/trace.h>
#include <math/derive.h>

int main() {
    futils::wrap::stack_trace_entry ent[10];
    auto entries = futils::wrap::get_stack_trace(ent);
    futils::wrap::get_symbols(
        entries, [](const char* s) {
            ;
        });
}
