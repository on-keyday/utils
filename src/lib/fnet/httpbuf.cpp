/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/dll/httpbuf.h>
#include <fnet/dll/httpbufproxy.h>
#include <fnet/dll/glheap.h>

namespace utils {
    namespace fnet {
        /*
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
        }*/
    }  // namespace fnet
}  // namespace utils