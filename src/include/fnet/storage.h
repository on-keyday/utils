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
    namespace fnet {
        namespace internal {
            struct Delete {
                void operator()(byte* data, size_t size) const {
                    free_normal(data, DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
                }

                template <class T>
                void operator()(T* data) const {
                    delete_glheap(data);
                }
            };
        }  // namespace internal

        using storage = view::storage_vec<internal::Delete>;

        inline storage make_storage(size_t size) {
            auto dst = (byte*)alloc_normal(size, alignof(byte), DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
            if (!dst) {
                // maybe throw
                memory_exhausted_traits(DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
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

    }  // namespace fnet
}  // namespace utils
