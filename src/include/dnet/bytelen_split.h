/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// bytelen_split - split util
#pragma once
#include "bytelen.h"

namespace utils {
    namespace dnet {
        namespace spliter {
            // (void or bool) result_fn(ByteLen,size_t offset,size_t remain)
            // size_t length_fn(size_t total,size_t remain,size_t prev)
            // length_fn should return least 1
            template <class B>
            constexpr bool split(ByteLenBase<B> b, auto&& length_fn, auto&& result_fn) {
                ByteLenBase<B> fwd = b;
                size_t prev = 0;
                while (fwd.len) {
                    size_t splt = length_fn(b.len, fwd.len, prev);
                    if (splt < 1 || fwd.len < splt) {
                        return false;
                    }
                    size_t offset = b.len - fwd.len;
                    auto res = fwd.resized(splt);
                    fwd = fwd.forward(splt);
                    if constexpr (internal::has_logical_not<decltype(result_fn(res, offset, fwd.len))>) {
                        if (!result_fn(res, offset, fwd.len)) {
                            return false;
                        }
                    }
                    else {
                        result_fn(res, offset);
                    }
                    prev = splt;
                }
                return true;
            }

            constexpr auto push_back_fn(auto& vec) {
                return [&](auto b, size_t off) {
                    vec.push_back({b, off});
                };
            }

            constexpr auto const_len_fn(size_t l) {
                return [=](size_t total, size_t remain, size_t prev) {
                    if (remain < l) {
                        return remain;
                    }
                    return l;
                };
            }

        }  // namespace spliter
    }      // namespace dnet
}  // namespace utils
