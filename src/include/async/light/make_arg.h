/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// make_arg - make argument object
#pragma once
#include <type_traits>

namespace utils {
    namespace async {
        namespace light {
            template <class... InArg>
            struct Args;

            template <class T>
            struct AnArg {
                using remref_t = std::remove_cvref_t<T>;
                remref_t value;
                constexpr AnArg(const T& t)
                    : value(t) {}

                constexpr const remref_t& get() {
                    return value;
                }
            };

            template <class T>
            struct AnArg<T*> {
                T* value;
                constexpr AnArg(T* t)
                    : value(t) {}
                constexpr T* get() {
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

                constexpr remref_t&& get() {
                    return std::move(value);
                }
            };

            template <class T>
            struct AnArg<T&> {
                T* value;

                constexpr AnArg(T& t)
                    : value(std::addressof(t)) {}
                constexpr T& get() {
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
                template <class T, class... V>
                constexpr Args(T&& one, Args<V...>&& v)
                    : one(std::forward<T>(one)), other(std::forward<Args<V...>>(v)) {}
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

            template <class One, class... Arg>
            Args(One, Args<Arg...>) -> Args<One, Arg...>;

            template <class T>
            struct array_to_pointer {
                static constexpr bool is_array = std::is_array_v<std::remove_reference_t<T>>;
                static constexpr bool is_not_pointer = !std::is_pointer_v<std::remove_reference_t<T>>;
                using Target = std::remove_extent_t<std::remove_reference_t<T>>*;
                using Type = std::conditional_t<is_array && is_not_pointer,
                                                Target,
                                                T>;
                static constexpr bool is_const = std::is_const_v<std::remove_reference_t<Type>>;
                using type = std::conditional_t<is_not_pointer && is_const,
                                                std::remove_cvref_t<Type>,
                                                Type>
            };

            template <class T>
            using array_to_pointer_t = typename array_to_pointer<T>::type;

            auto make_arg() {
                return Args<>{};
            }

            template <class One, class... Arg>
            auto make_arg(One&& one, Arg&&... arg) {
                return Args<array_to_pointer_t<One>, array_to_pointer_t<Arg>...>{std::forward<One>(one), make_arg(std::forward<Arg>(arg)...)};
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
