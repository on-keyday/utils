/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/freestd_macro.h>
#include <freestd/stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

void FUTILS_FREESTD_STDC(exit)(int status);
void FUTILS_FREESTD_STDC(abort)(void);
void* FUTILS_FREESTD_STDC(malloc)(size_t size);
void* FUTILS_FREESTD_STDC(calloc)(size_t nmemb, size_t size);
void* FUTILS_FREESTD_STDC(realloc)(void* ptr, size_t size);
void FUTILS_FREESTD_STDC(free)(void* ptr);

// implementation internal
typedef struct futils_MemoryPool {
    void* (*malloc)(futils_MemoryPool* ctx, size_t size);
    void* (*calloc)(futils_MemoryPool* ctx, size_t nmemb, size_t size);
    void* (*realloc)(futils_MemoryPool* ctx, void* ptr, size_t size);
    void (*free)(futils_MemoryPool* ctx, void* ptr);
} futils_MemoryPool;
void* FUTILS_FREESTD_STDC(pool_malloc)(futils_MemoryPool* pool, size_t size);
void* FUTILS_FREESTD_STDC(pool_calloc)(futils_MemoryPool* pool, size_t nmemb, size_t size);
void* FUTILS_FREESTD_STDC(pool_realloc)(futils_MemoryPool* pool, void* ptr, size_t size);
void FUTILS_FREESTD_STDC(pool_free)(futils_MemoryPool* pool, void* ptr);

futils_MemoryPool* FUTILS_FREESTD_STDC(global_pool)();

#ifdef __cplusplus
}
#endif
