/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#ifdef __wasi__
#include <cstdlib>

extern "C" void* __cxa_allocate_exception(size_t thrown_size) noexcept {
    abort();
}

extern "C" void __cxa_throw(void* thrown_exception, void* tinfo, void (*dest)(void*)) {
    abort();
}
#endif
