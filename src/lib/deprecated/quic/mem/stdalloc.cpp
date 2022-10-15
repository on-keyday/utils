/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <deprecated/quic/common/dll_cpp.h>
#include <deprecated/quic/mem/alloc.h>
#include <cstdlib>

namespace utils {
    namespace quic {
        namespace allocate {
            dll(Alloc) stdalloc() {
                allocate::Alloc alloc{};
                alloc.alloc = [](void*, size_t s, size_t) { return ::malloc(s); };
                alloc.resize = [](void*, void* p, size_t s, size_t) { return ::realloc(p, s); };
                alloc.put = [](void*, void* p) { return ::free(p); };
                alloc.C = nullptr;
                return alloc;
            }
        }  // namespace allocate
    }      // namespace quic
}  // namespace utils
