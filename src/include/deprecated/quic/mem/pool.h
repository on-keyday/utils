/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// pool - object pool
#pragma once
#include "alloc.h"
#include <utility>

namespace utils {
    namespace quic {
        namespace pool {

            struct BytesHolder {
                void* h;
                bytes::Bytes (*get_)(void*, tsize len, allocate::Alloc* a);
                void (*put_)(void*, bytes::Bytes b, allocate::Alloc* a);
                void (*free_)(void*, allocate::Alloc* a);
            };

            struct BytesPool {
                allocate::Alloc* a;
                BytesHolder holder;
                constexpr BytesPool() {}
                BytesPool(allocate::Alloc* a, BytesHolder holder)
                    : a(a), holder(holder) {}
                bytes::Bytes get(tsize len) {
                    if (len == 0 || !holder.get_) {
                        return {};
                    }
                    auto got = holder.get_(holder.h, len, a);
                    if (got.size() < len) {
                        a->expand_buffer(got, len);
                    }
                    return got;
                }

                void put(bytes::Bytes&& b) {
                    if (!b.own()) {
                        return;
                    }
                    holder.put_(holder.h, std::move(b), a);
                }

                ~BytesPool() {
                    holder.free_(holder.h, a);
                }
            };

            struct RAIIBytes {
                bytes::Bytes b;
                BytesPool* pool;
                RAIIBytes(tsize size, BytesPool* p) {
                    b = p->get(size);
                    pool = p;
                }

                byte* get() const {
                    return b.own();
                }

                tsize size() const {
                    return b.size();
                }

                const byte* c_str() const {
                    return b.c_str();
                }

                ~RAIIBytes() {
                    pool->put(std::move(b));
                }
            };

            namespace concepts {
                template <class T>
                concept ByteSeq = std::is_same_v < std::remove_cvref_t<T>,
                char* >
                    || std::is_same_v<std::remove_cvref_t<T>, byte*>;

                template <class T>
                concept CStr = requires(T t) {
                    { t.c_str() } -> ByteSeq;
                };
            }  // namespace concepts

            template <class E, E ok, E err>
            E select_bytes_way(auto&& bytes, pool::BytesPool& bpool, tsize size, bool cond, tsize offset, auto&& callback) {
                using Bytes = std::remove_cvref_t<decltype(bytes)>;
                if constexpr (concepts::ByteSeq<Bytes>) {
                    auto v = reinterpret_cast<const byte*>(bytes);
                    return callback(bytes + offset);
                }
                else if constexpr (concepts::CStr<Bytes>) {
                    return callback(bytes.c_str() + offset);
                }
                else {
                    if (cond) {
                        RAIIBytes tmp{size, &bpool};
                        byte* target = tmp.get();
                        if (!target) {
                            return err;
                        }
                        bytes::copy(target, bytes, size, offset);
                        return callback(tmp.c_str());
                    }
                    return ok;
                }
            }
        }  // namespace pool
    }      // namespace quic
}  // namespace utils
