/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <freestd/stdlib.h>
#include <freestd/cstdint>

extern "C" {
void* FUTILS_FREESTD_STDC(pool_malloc)(futils_MemoryPool* pool, size_t size) {
    return pool->malloc(pool, size);
}

void* FUTILS_FREESTD_STDC(pool_calloc)(futils_MemoryPool* pool, size_t nmemb, size_t size) {
    return pool->calloc(pool, nmemb, size);
}

void* FUTILS_FREESTD_STDC(pool_realloc)(futils_MemoryPool* pool, void* ptr, size_t size) {
    return pool->realloc(pool, ptr, size);
}

void FUTILS_FREESTD_STDC(pool_free)(futils_MemoryPool* pool, void* ptr) {
    pool->free(pool, ptr);
}
// platform specific
futils_MemoryPool* futils_platform_global_pool();

futils_MemoryPool* FUTILS_FREESTD_STDC(global_pool)() {
    return futils_platform_global_pool();
}

void* FUTILS_FREESTD_STDC(malloc)(size_t size) {
    return FUTILS_FREESTD_STDC(pool_malloc)(FUTILS_FREESTD_STDC(global_pool)(), size);
}

void* FUTILS_FREESTD_STDC(calloc)(size_t nmemb, size_t size) {
    return FUTILS_FREESTD_STDC(pool_calloc)(FUTILS_FREESTD_STDC(global_pool)(), nmemb, size);
}

void* FUTILS_FREESTD_STDC(realloc)(void* ptr, size_t size) {
    return FUTILS_FREESTD_STDC(pool_realloc)(FUTILS_FREESTD_STDC(global_pool)(), ptr, size);
}

void FUTILS_FREESTD_STDC(free)(void* ptr) {
    FUTILS_FREESTD_STDC(pool_free)
    (FUTILS_FREESTD_STDC(global_pool)(), ptr);
}

}  // extern "C"
