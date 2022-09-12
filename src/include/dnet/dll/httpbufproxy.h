/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpbufproxy - proxy class
#pragma once
#include "httpbuf.h"

namespace utils {
    namespace dnet {
        struct HTTPConstBufProxy {
            const HTTPBuf& buf;

            char* const& text() {
                return buf.text;
            }
            const size_t& size() {
                return buf.size;
            }
            const size_t& cap() {
                return buf.cap;
            }
        };

        struct HTTPBufProxy {
            HTTPBuf& buf;
            void inisize(size_t len) {
                buf = {len};
            }
            char*& text() {
                return buf.text;
            }
            size_t& size() {
                return buf.size;
            }
            size_t& cap() {
                return buf.cap;
            }
        };
    }  // namespace dnet
}  // namespace utils
