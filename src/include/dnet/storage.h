/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// storage - size fit/flexible storage
#pragma once
#include "../view/iovec.h"
#include "../view/expand_iovec.h"
#include "dll/glheap.h"
#include "dll/allocator.h"

namespace utils {
    namespace dnet {
        namespace internal {
            struct Delete {
                void operator()(byte* data, size_t size) const {
                    free_normal(data, DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
                }

                template <class T>
                void operator()(T* data) const {
                    delete_with_global_heap(data, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T)));
                }
            };
        }  // namespace internal

        using storage = view::storage_vec<internal::Delete>;

        inline storage make_storage(size_t size) {
            auto dst = (byte*)alloc_normal(size, alignof(byte), DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
            if (!dst) {
                return {};
            }
            return storage(dst, size);
        }

        inline storage make_storage(const byte* src, size_t size) {
            auto dst = make_storage(size);
            if (dst.null()) {
                return {};
            }
            for (auto i = 0; i < size; i++) {
                dst[i] = src[i];
            }
            return dst;
        }

        using flex_storage = view::expand_storage_vec<glheap_allocator<byte>>;

    }  // namespace dnet
}  // namespace utils
