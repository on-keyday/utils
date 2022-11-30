/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/byte.h>
namespace utils {
    namespace dnet {

        static void clrmem(void* p, size_t len) {
            auto m = (byte*)p;
            for (size_t i = 0; i < len; i++) {
                m[i] = 0;
            }
        }

        decltype(clrmem)* xclrmem_ = clrmem;

    }  // namespace dnet
}  // namespace utils
