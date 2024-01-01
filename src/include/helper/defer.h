/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// defer - defer call
#pragma once
#include <type_traits>
#include <utility>

namespace futils {
    namespace helper {

        template <class Fn>
        struct Defer {
           private:
            Fn fn;
            bool own;

           public:
            template <class F>
            constexpr Defer(F&& f)
                : fn(std::move(f)), own(true) {}

            constexpr Defer(Defer&& d)
                : fn(std::move(d.fn)), own(std::exchange(d.own, false)) {}

            // cancel deferred execution
            constexpr void cancel() {
                own = false;
            }

            constexpr void execute() {
                if (own) {
                    fn();
                }
                own = false;
            }

            constexpr ~Defer() {
                if (own) {
                    fn();
                }
            }
        };

        // like golang defer statement
        // you shouldn't discard return value
        [[nodiscard]] constexpr auto defer(auto&& fn) {
            return Defer<std::decay_t<decltype(fn)>>(std::forward<decltype(fn)>(fn));
        }

        // initializer helper method
        [[nodiscard]] constexpr auto init(auto&& fn) {
            fn();
            return true;
        }

    }  // namespace helper
}  // namespace futils
