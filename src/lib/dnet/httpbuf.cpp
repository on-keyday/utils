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
            free_cvec(text);
            text = nullptr;
        }

        HTTPBuf::HTTPBuf(size_t c) {
            text = get_cvec(c);
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
                if (!resize_cvec(p.text(), p.cap() * 2)) {
                    return;
                }
                p.cap() <<= 1;
            }
            p.text()[p.size()] = c;
            p.size()++;
        }
    }  // namespace dnet
}  // namespace utils
