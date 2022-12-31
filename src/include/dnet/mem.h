/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// mem - memory util
#pragma once
#include "dll/dllh.h"
#include "../core/byte.h"
namespace utils {
    namespace dnet {

        extern void (*xclrmem_)(void*, size_t);

        inline void clear_memory(void* p, size_t len) {
            xclrmem_(p, len);
        }
    }  // namespace dnet
}  // namespace utils
