/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bytes - byte interface
#pragma once
#include "../doc.h"
#include "../common/macros.h"

namespace utils {
    namespace quic {
        namespace allocate {
            struct Alloc;
        }

        namespace bytes {
            struct Bytes {
               private:
                friend struct allocate::Alloc;
                byte* buf;
                tsize len;
                bool copy;
                constexpr Bytes(byte* b, tsize l, bool c)
                    : buf(b), len(l), copy(c) {}

               public:
                constexpr Bytes()
                    : buf(nullptr), len(0), copy(false) {}

                constexpr Bytes(const Bytes& b)
                    : buf(b.buf), len(b.len), copy(true) {}

                constexpr Bytes(Bytes&& b)
                    : buf(b.buf), len(b.len), copy(b.copy) {
                    b.copy = true;
                }

                byte* own() const {
                    if (copy) {
                        return nullptr;
                    }
                    return buf;
                }

                const byte* borrow() const {
                    return buf;
                }

                const byte* c_str() const {
                    return buf;
                }

                constexpr ~Bytes() {
                    if (!copy && buf) {
                        illegal();
                    }
                }

                constexpr tsize size() const {
                    return len;
                }

                constexpr Bytes& operator=(Bytes&& left) {
                    if (this == &left) {
                        return *this;
                    }
                    buf = left.buf;
                    len = left.len;
                    copy = left.copy;
                    left.copy = true;
                    return *this;
                }

                constexpr Bytes& operator=(const Bytes& left) {
                    if (this == &left) {
                        return *this;
                    }
                    buf = left.buf;
                    len = left.len;
                    copy = true;
                    return *this;
                }

                constexpr byte operator[](tsize i) const {
                    if (!buf || i >= len) {
                        return 0;
                    }
                    return buf[i];
                }

                constexpr Bytes operator()() const {
                    return Bytes{buf, len, true};
                }

                constexpr Bytes operator()(tsize begin) const {
                    if (begin > len) {
                        begin = len;
                    }
                    return Bytes{buf + begin, len - begin, true};
                }

                constexpr Bytes operator()(tsize begin, tsize end) const {
                    if (begin > len) {
                        begin = len;
                    }
                    if (end > len) {
                        end = len;
                    }
                    if (end < begin) {
                        return Bytes{buf + len, 0, true};
                    }
                    return Bytes{buf + begin, end - begin, true};
                }
            };

            // copy copys data from src to dst
            // initial:i=offset
            // condition:i<len
            // operation:dst[i-offset]=src[i]
            constexpr void copy(auto& dst, auto& src, tsize len, tsize offset = 0) {
                for (tsize i = offset; i < len; i++) {
                    dst[i - offset] = src[i];
                }
            }

        }  // namespace bytes
    }      // namespace quic
}  // namespace utils