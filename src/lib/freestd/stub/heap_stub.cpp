/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <freestd/stdlib.h>
#include "../simple_alloc.h"

#ifdef FUTILS_FREESTD_HEAP_MEMORY_SIZE
alignas(alignof(uintptr_t)) char futils_freestd_heap_memory[FUTILS_FREESTD_HEAP_MEMORY_SIZE];
struct SimplePool : futils_MemoryPool {
    futils::freestd::ChunkList chunk;

    SimplePool() {
        chunk.init_heap(futils_freestd_heap_memory);
        this->malloc = [](futils_MemoryPool* ctx, size_t size) -> void* {
            return static_cast<SimplePool*>(ctx)->chunk.malloc(size);
        };
        this->calloc = [](futils_MemoryPool* ctx, size_t nmemb, size_t size) -> void* {
            return static_cast<SimplePool*>(ctx)->chunk.calloc(nmemb, size);
        };
        this->realloc = [](futils_MemoryPool* ctx, void* ptr, size_t size) -> void* {
            return static_cast<SimplePool*>(ctx)->chunk.realloc(ptr, size);
        };
        this->free = [](futils_MemoryPool* ctx, void* ptr) -> void {
            static_cast<SimplePool*>(ctx)->chunk.free(ptr);
        };
    }
};

// stub for runtime initialization, do not use in multi-threaded environment
FUTILS_FREESTD_STUB_WEAK
extern "C" int __cxa_guard_acquire(uint64_t* p) {
    if (*p) {
        return 0;
    }
    *p = 1;
    return 1;
}
FUTILS_FREESTD_STUB_WEAK
extern "C" void __cxa_guard_release(uint64_t*) {}

FUTILS_FREESTD_STUB_WEAK
extern "C" futils_MemoryPool* futils_platform_global_pool() {
    static SimplePool simple_pool;
    return &simple_pool;
}

#else
FUTILS_FREESTD_STUB_WEAK
extern "C" futils_MemoryPool* futils_platform_global_pool() {
    static futils_MemoryPool null_memory_pool = {
        .malloc = [](futils_MemoryPool*, size_t) -> void* {
            return nullptr;
        },
        .calloc = [](futils_MemoryPool*, size_t, size_t) -> void* {
            return nullptr;
        },
        .realloc = [](futils_MemoryPool*, void*, size_t) -> void* {
            return nullptr;
        },
        .free = [](futils_MemoryPool*, void*) -> void {
        },
    };
    return &null_memory_pool;
}
#endif
