/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// alloc - allocation of libquic
#pragma once
#include "../common/dll_h.h"
#include "../doc.h"
#include "../common/macros.h"
#include "bytes.h"
#include <utility>
#include <memory>

namespace utils {
    namespace quic {
        namespace allocate {

            struct Alloc {
                void* C;
                void* (*alloc)(void* C, tsize size, tsize align);
                void* (*resize)(void* C, void* ptr, tsize new_size, tsize align);
                void (*put)(void* C, void* ptr);
                void (*free)(void* C);

                template <class T, class... Arg>
                T* allocate(Arg&&... arg) {
                    auto ptr = alloc(C, sizeof(T), alignof(T));
                    if (!ptr) {
                        return nullptr;
                    }
                    return new (ptr) T(std::forward<Arg>(arg)...);
                }

                template <class T>
                void deallocate(T* ptr) noexcept {
                    if (ptr) {
                        // for self destruction
                        auto put_f = put;
                        auto C_ = C;
                        ptr->~T();
                        put_f(C_, ptr);
                    }
                }

                template <class T>
                T* allocate_vec(tsize vlen) {
                    auto ptr = alloc(C, sizeof(T) * vlen, alignof(T));
                    auto vec = static_cast<T*>(ptr);
                    for (tsize i = 0; i < vlen; i++) {
                        new (std::addressof(vec[i])) T{};
                    }
                    return vec;
                }

                template <class T>
                T* resize_vec(T* current, tsize curlen, tsize newlen) {
                    if (curlen == newlen) {
                        return current;
                    }
                    auto ptr = alloc(C, sizeof(T) * newlen, alignof(T));
                    if (!ptr) {
                        return nullptr;
                    }
                    auto vec = static_cast<T*>(ptr);
                    for (tsize i = 0; i < newlen; i++) {
                        new (std::addressof(vec[i])) T();
                    }
                    auto mi = curlen < newlen ? curlen : newlen;
                    auto ma = newlen < curlen ? curlen : newlen;
                    for (tsize i = 0; i < mi; i++) {
                        vec[i] = std::move(current[i]);
                    }
                    for (tsize i = 0; i < curlen; i++) {
                        current[i].~T();
                    }
                    put(C, current);
                    return vec;
                }

                template <class T>
                void deallocate_vec(T* ptr, tsize vlen) {
                    if (ptr) {
                        for (tsize i = 0; i < vlen; i++) {
                            ptr[i].~T();
                        }
                        put(C, ptr);
                    }
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

            Dll(Alloc) stdalloc();

        }  // namespace allocate

        namespace bytes {
            inline int append(bytes::Bytes& b, tsize* blen, allocate::Alloc* a, const byte* data, tsize len) {
                if (!b.own() && b.c_str() || !blen || !a) {
                    return 0;
                }
                if (!data) {
                    return len == 0 ? 1 : 0;
                }
                if (b.size() < *blen + len) {
                    if (!a->expand_buffer(b, *blen + len + 2)) {
                        return -1;
                    }
                }
                auto o = b.own();
                if (!o) {
                    return -1;
                }
                for (tsize i = 0; i < len; i++) {
                    o[*blen + i] = data[i];
                }
                *blen += len;
                return 1;
            }

            inline int append(bytes::Buffer& b, const byte* data, tsize len) {
                return append(b.b, &b.len, b.a, data, len);
            }

        }  // namespace bytes
    }      // namespace quic
}  // namespace utils
