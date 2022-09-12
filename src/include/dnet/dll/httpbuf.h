/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// httpbuf - http buffer
#pragma once
#include "dllh.h"
#include <cstddef>
#include <utility>
namespace utils {
    namespace dnet {
        struct HTTPBufProxy;
        struct dnet_class_export HTTPBuf {
           private:
            char* text = nullptr;
            size_t size = 0;
            size_t cap = 0;
            friend struct HTTPBufProxy;
            friend struct HTTPConstBufProxy;

            HTTPBuf(size_t c);

           public:
            constexpr HTTPBuf() = default;
            constexpr HTTPBuf(HTTPBuf&& buf)
                : text(std::exchange(buf.text, nullptr)),
                  size(std::exchange(buf.size, 0)),
                  cap(std::exchange(buf.cap, 0)) {}
            constexpr HTTPBuf& operator=(HTTPBuf&& buf) {
                if (this == &buf) {
                    return *this;
                }
                this->~HTTPBuf();
                text = std::exchange(buf.text, nullptr);
                size = std::exchange(buf.size, 0);
                cap = std::exchange(buf.cap, 0);
                return *this;
            }
            constexpr size_t getsize() const {
                return size;
            }
            constexpr size_t getcap() const {
                return cap;
            }
            ~HTTPBuf();
        };

        struct dnet_class_export HTTPPushBack {
           private:
            HTTPBuf* buf = nullptr;

           public:
            constexpr HTTPPushBack(HTTPBuf* b)
                : buf(b) {}
            void push_back(unsigned char c);
        };

    }  // namespace dnet
}  // namespace utils
