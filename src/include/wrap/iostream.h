/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>
#include <binary/writer.h>
#include <istream>
#include <ostream>

namespace futils::wrap {
    template <class C>
    struct iostream_adapter {
        static constexpr binary::ReadStreamHandlerT<C, std::istream> in{
            .base = {
                .empty = [](void* ctx, size_t offset) -> bool {
                    auto self = static_cast<std::istream*>(ctx);
                    return self->eof() || self->fail();
                },
                .direct_read = [](void* ctx, view::basic_wvec<C> w, size_t expected) -> size_t {
                    auto self = static_cast<std::istream*>(ctx);
                    self->read(reinterpret_cast<char*>(w.data()), w.size());
                    return self->gcount();
                },
            },
        };

        static constexpr binary::WriteStreamHandlerT<C, std::ostream> out{
            .base = {
                .full = [](void* ctx, size_t offset) -> bool {
                    auto self = static_cast<std::ostream*>(ctx);
                    return self->fail();
                },
                .direct_write = [](void* ctx, view::basic_rvec<C> r, size_t times) {
                    auto self = static_cast<std::ostream*>(ctx);
                    for (size_t i = 0; i < times; i++) {
                        self->write(reinterpret_cast<const char*>(r.data()), r.size());
                        if(self->fail()) {
                            return false;
                        }
                    } 
                    return true; },
            },
        };
    };
}  // namespace futils::wrap
