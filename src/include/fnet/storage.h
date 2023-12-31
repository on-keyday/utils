/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// storage - size fit/flexible storage
#pragma once
#include "../view/iovec.h"
#include "../view/expand_iovec.h"
#include "dll/glheap.h"
#include "dll/allocator.h"
#include <binary/reader.h>

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

        constexpr auto sizeof_storage = sizeof(storage);

        inline storage make_storage(size_t size) {
            auto dst = (byte*)alloc_normal(size, alignof(byte), DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
            if (!dst) {
                // maybe throw
                memory_exhausted_traits(DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
                return {};
            }
            return storage(view::wvec(dst, size));
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

        constexpr auto sizeof_flex = sizeof(flex_storage);

        template <class C>
        constexpr bool read_flex(binary::basic_reader<C>& r, flex_storage& s, size_t to_read) {
            if (r.remain().size() < to_read) {
                return false;
            }
            if (to_read > s.max_size()) {
                return false;
            }
            s.resize(to_read);
            s.shrink_to_fit();  // for sso optimization
            return r.read(s);
        }

    }  // namespace fnet
}  // namespace utils
