/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// mem - memory buffer
#pragma once
#include "cnet.h"
#include <cstring>

namespace utils {
    namespace cnet {
        namespace mem {
            struct MemoryBuffer;
            struct CustomAllocator {
                void* (*alloc)(void*, size_t) = nullptr;
                void* (*realloc)(void*, void*, size_t) = nullptr;
                void (*deleter)(void*, void*) = nullptr;
                void (*ctxdeleter)(void*) = nullptr;
                void* ctx = nullptr;
            };
            DLL MemoryBuffer* STDCALL new_buffer(CustomAllocator allocs);
            DLL MemoryBuffer* STDCALL new_buffer(void* (*alloc)(size_t), void* (*realloc)(void*, size_t), void (*deleter)(void*));
            DLL void STDCALL delete_buffer(MemoryBuffer* buf);
            DLL size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s);
            inline size_t append(MemoryBuffer* b, const char* m) {
                return append(b, m, m ? ::strlen(m) : 0);
            }
            DLL size_t STDCALL remove(MemoryBuffer* b, void* m, size_t s);

            DLL bool STDCALL clear_and_allocate(MemoryBuffer* b, size_t new_size, bool fixed);
        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
