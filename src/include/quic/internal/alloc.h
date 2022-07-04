/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// alloc - allocation of libquic
#pragma once
#include "../doc.h"
#include "../common/macros.h"
#include "bytes.h"

namespace utils {
    namespace quic {
        namespace allocate {

            struct Alloc {
                void* C;
                void* (*alloc)(void* C, tsize size, tsize align);
                void* (*resize)(void* C, void* ptr, tsize new_size, tsize align);
                void (*put)(void* C, void* ptr);

                template <class T>
                T* allocate() {
                    auto ptr = alloc(C, sizeof(T), alignof(T));
                    return new (ptr) T{};
                }

                template <class T>
                void deallocate(T* ptr) {
                    put(C, ptr);
                }

                bytes::Bytes make_buffer(tsize len) {
                    auto b = reinterpret_cast<byte*>(alloc(C, len, 1));
                    return bytes::Bytes{b, len, false};
                }

                // expand_buffer expands buffer
                bool expand_buffer(bytes::Bytes& b, tsize size) {
                    if (b.copy || size < b.len) {
                        return false;
                    }
                    auto resized = resize(C, b.buf, size, 1);
                    if (resized == nullptr) {
                        return false;
                    }
                    b.len = size;
                    if (resized != b.buf) {
                        b.buf = reinterpret_cast<byte*>(resized);
                    }
                    return true;
                }

                void discard_buffer(bytes::Bytes& b) {
                    if (b.copy) {
                        return;
                    }
                    put(C, b.buf);
                    b.buf = nullptr;
                    b.len = 0;
                    b.copy = true;
                }
            };

        }  // namespace allocate
    }      // namespace quic
}  // namespace utils
