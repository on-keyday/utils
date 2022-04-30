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
            DLL MemoryBuffer* STDCALL new_buffer(void* (*alloc)(size_t), void* (*realloc)(void*, size_t), void (*deleter)(void*));
            DLL void STDCALL delete_buffer(MemoryBuffer* buf);
            DLL size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s);
            inline size_t append(MemoryBuffer* b, const char* m) {
                return append(b, m, m ? ::strlen(m) : 0);
            }
            DLL size_t STDCALL remove(MemoryBuffer* b, void* m, size_t s);
        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
