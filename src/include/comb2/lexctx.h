/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "pos.h"
#include "status.h"
#include <concepts>
#include "../strutil/append.h"

namespace utils::comb2 {
    struct no_errbuf_t {};
    template <class Tag = const char*, class ErrBuf = no_errbuf_t>
    struct LexContext {
        Tag str_tag = Tag{};
        Pos str_pos;
        ErrBuf errbuf{};

        constexpr void end_string(Status res, auto&& tag, auto&& seq, Pos pos) {
            if (res == Status::match) {
                str_pos = pos;
                if constexpr (std::convertible_to<decltype(tag), Tag>) {
                    str_tag = tag;
                }
            }
        }

        constexpr void error_seq(auto& seq, auto&&... err) {
            error(err...);
        }

        constexpr void error(auto&&... err) {
            if constexpr (!std::is_same_v<ErrBuf, no_errbuf_t>) {
                auto add = [&](auto&& v) {
                    if constexpr (std::is_integral_v<std::decay_t<decltype(v)>>) {
                        errbuf.push_back(v);
                    }
                    else if constexpr (ctxs::has_to_string<decltype(v)>) {
                        strutil::append(errbuf, to_string(v));
                    }
                    else {
                        strutil::append(errbuf, v);
                    }
                };
                (..., add(err));
            }
        }
    };
}  // namespace utils::comb2
