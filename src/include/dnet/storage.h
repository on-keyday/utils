/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

        inline storage make_storage(view::rvec src) {
            auto dst = make_storage(src.size());
            if (dst.null()) {
                return {};
            }
            view::copy(dst, src);
            return dst;
        }

        using flex_storage = view::expand_storage_vec<glheap_allocator<byte>>;

    }  // namespace dnet
}  // namespace utils
