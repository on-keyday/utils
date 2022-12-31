/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/httpbuf.h>
#include <dnet/dll/httpbufproxy.h>
#include <dnet/dll/glheap.h>

namespace utils {
    namespace dnet {
        HTTPBuf::~HTTPBuf() {
            free_charvec(text, DNET_DEBUG_MEMORY_LOCINFO(true, cap, alignof(char)));
            text = nullptr;
        }

        HTTPBuf::HTTPBuf(size_t c) {
            text = get_charvec(c, DNET_DEBUG_MEMORY_LOCINFO(true, c, alignof(char)));
            if (text) {
                cap = c;
            }
        }

        void HTTPPushBack::push_back(unsigned char c) {
            if (!buf) {
                return;
            }
            HTTPBufProxy p{*buf};
            if (p.size() + 1 >= p.cap()) {
                if (!resize_charvec(p.text(), p.cap() * 2, DNET_DEBUG_MEMORY_LOCINFO(true, p.cap(), alignof(char)))) {
                    return;
                }
                p.cap() <<= 1;
            }
            p.text()[p.size()] = c;
            p.size()++;
        }
    }  // namespace dnet
}  // namespace utils
