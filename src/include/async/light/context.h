/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - light context
#pragma once
#include <type_traits>

namespace utils {
    namespace async {
        namespace light {
            struct Context;
#ifdef _WIN32
#define ASYNC_NO_VTABLE_ __declspec(novtable)
#else
#define ASYNC_NO_VTABLE_
#endif
            struct ASYNC_NO_VTABLE_ Executor {
                virtual void execute(Context) = 0;
                virtual ~Executor() {}
            };

            template <class T>
            constexpr bool is_ref_v =
                std::conjunction_v<std::is_lvalue_reference<T>, std::negation<std::is_const<T>>>;

            template <class... InArg>
            struct Args;

            template <class T>
            struct AnArg {
                using remref_t = std::conditional_t<std::is_array_v<T>,
                                                    std::conditional_t<
                                                        std::is_const_v<T>,
                                                        std::decay_t<T>,
                                                        std::add_const_t<std::decay_t<T>>>,
                                                    std::remove_cvref_t<T>>;
                remref_t value;
                constexpr AnArg(const T& t)
                    : value(t) {}

                const remref_t& get() {
                    return value;
                }
            };

            template <class T>
            struct AnArg<T&&> {
                using remref_t =
                    std::remove_cvref_t<T>;
                remref_t value;
                constexpr AnArg(T&& t)
                    : value(std::move(t)) {}

                remref_t&& get() {
                    return std::move(value);
                }
            };

            template <class T>
            struct AnArg<T&> {
                T* value;

                constexpr AnArg(T& t)
                    : value(std::addressof(t)) {}
                T& get() {
                    return *value;
                }
            };

            template <>
            struct Args<> {
                constexpr Args() {}
            };
            template <class One, class... Other>
            struct Args<One, Other...> {
                AnArg<One> one;
                Args<Other...> other;
                constexpr Args(AnArg<One> one, Args<Other...>&& v)
                    : one(std::forward<One>(one)), other(std::forward<Args<Other...>>(v)) {}
                template <size_t index, size_t current = 0>
                decltype(auto) get() {
                    if constexpr (index == current) {
                        return one.get();
                    }
                    else {
                        return other.template get<index, current + 1>();
                    }
                }
            };

            auto make_arg() {
                return Args<>{};
            }

            template <class One, class... Arg>
            Args(AnArg<One>, Args<Arg...>) -> Args<One, Arg...>;

            template <class One, class... Arg>
            auto make_arg(One&& one, Arg&&... arg) {
                return Args{AnArg{std::forward<One>(one)}, make_arg(std::forward<Arg>(arg)...)};
            }

            struct Context {
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
