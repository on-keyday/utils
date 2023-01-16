/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ctxbase - context based controll
#pragma once

namespace utils {
    namespace minilang::token {
        // bool judge(src)
        constexpr auto ctx_enter(auto&& yielder, auto&& judge) {
            return [=](auto&& src) {
                using Result = decltype(yielder(src));
                if (!judge(src)) {
                    return Result(nullptr);
                }
                return yielder(src);
            };
        }

        // bool judge(token,src)
        constexpr auto ctx_leave(auto&& yielder, auto&& judge) {
            return [=](auto&& src) {
                auto res = yielder(src);
                if (!judge(res, src)) {
                    return decltype(yielder(src))(nullptr);
                }
                return res;
            };
        }
        // bool enter(src)
        // bool leave(token,src)
        constexpr auto ctx_scope(auto&& enter, auto&& inscope, auto&& leave) {
            return ctx_leave(ctx_enter(inscope, enter), leave);
        }

        constexpr auto enabler(auto&& yielder, bool enable) {
            return [=](auto&& src) {
                if (!enable) {
                    return decltype(yielder(src))(nullptr);
                }
                return yielder(src);
            };
        }
    }  // namespace minilang::token
}  // namespace utils
