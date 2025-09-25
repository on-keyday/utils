/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <core/sequencer.h>
#include "pos.h"
#include <type_traits>

namespace futils::comb2 {
    template <class T, class U>
    concept has_push_back = requires(T t, U u) {
        t.push_back(u);
    };
    template <class S, class A>
    concept has_substr_to = requires(S s) {
        { std::declval<A&>() = s.substr(size_t(1), size_t(1)) };
    };
    template <class Ident, class T>
    constexpr void seq_range_to_string(Ident& ident, const Sequencer<T>& seq, Pos pos) {
        if constexpr (has_substr_to<decltype(seq.buf.buffer), Ident>) {
            ident = seq.buf.buffer.substr(pos.begin, pos.end - pos.begin);
        }
        else {
            static_assert(has_push_back<Ident, decltype(seq.buf.buffer[1])>);
            for (auto i = pos.begin; i < pos.end; i++) {
                ident.push_back(seq.buf.buffer[i]);
            }
        }
    }
}  // namespace futils::comb2
