/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tcp - tcp cnet interface
#pragma once
#include <cstdint>

namespace utils {
    namespace cnet {
        namespace tcp {
            struct OsTCPSocket {
#ifdef _WIN32
                std::uintptr_t sock;
#else
                int sock;
#endif
            };

        }  // namespace tcp
    }      // namespace cnet
}  // namespace utils
