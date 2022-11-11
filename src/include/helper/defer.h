/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// defer - defer call
#pragma once
#include <type_traits>
#include <utility>
#include <tuple>

namespace utils {
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

        template <class Fn, class... Optional>
        struct WrapPtr {
            std::tuple<Optional...> optional;
            Fn fn;

            template <class F>
            constexpr WrapPtr(F&& f)
                : fn(std::move(f)) {}

            constexpr WrapPtr(WrapPtr&& d)
                : fn(std::move(d.fn)) {}

            constexpr auto execute(auto&&... args) {
                return fn(std::forward<decltype(args)>(args)...);
            }
        };

        template <class... Optional>
        [[nodiscard]] constexpr auto wrapfn(auto&& fn) {
            return WrapPtr<std::decay_t<decltype(fn)>, Optional...>(std::forward<decltype(fn)>(fn));
        }

        template <class... Optional>
        [[nodiscard]] constexpr auto wrapfn_ptr(auto&& fn, auto get_ptr, auto del_ptr) {
            using Wrap = WrapPtr<std::decay_t<decltype(fn)>, Optional...>;
            void* ptr = get_ptr(sizeof(Wrap), alignof(Wrap));
            if (!ptr) {
                return (Wrap*)nullptr;
            }
            auto r = helper::defer([&] {
                del_ptr(ptr);
            });
            auto ret = new (ptr) Wrap{std::forward<decltype(fn)>(fn)};
            r.cancel();
            return ret;
        }
    }  // namespace helper
}  // namespace utils
