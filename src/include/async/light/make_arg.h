/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// make_arg - make argument object
#pragma once
#include <type_traits>
#include <memory>

namespace utils {
    namespace async {
        namespace light {
            template <class... InArg>
            struct Args;

            enum class ArgTrait {
                normal,
                lref,
                ptr,
                void_,
            };

            template <class T>
            struct AnArg {
                using remref_t = std::remove_cvref_t<T>;
                remref_t value;
                static constexpr ArgTrait traits = ArgTrait::normal;

                constexpr AnArg() {}
                constexpr AnArg(T t)
                    : value(std::move(t)) {}

                constexpr remref_t&& get() {
                    return std::move(value);
                }
            };

            template <class T>
            struct AnArg<T*> {
                T* value;

                static constexpr ArgTrait traits = ArgTrait::ptr;
                constexpr AnArg() {}
                constexpr AnArg(T* t)
                    : value(t) {}
                constexpr T* get() {
                    return value;
                }
            };

            template <class T>
            struct AnArg<T&> {
                T* value;

                static constexpr ArgTrait traits = ArgTrait::lref;
                constexpr AnArg() {}
                constexpr AnArg(T& t)
                    : value(std::addressof(t)) {}
                constexpr T& get() {
                    return *value;
                }
            };

            template <>
            struct Args<> {
                constexpr Args() {}
                constexpr size_t size() const {
                    return 0;
                }

                template <class Fn>
                constexpr decltype(auto) invoke(Fn&& fn) {
                    return fn();
                }
            };

            template <>
            struct Args<void> {
                template <size_t index, size_t current = 0>
                constexpr void get() const {
                    static_assert(index == current, "void but require more value");
                }
            };

            template <class One, class... Other>
            struct Args<One, Other...> {
                AnArg<One> one;
                [[no_unique_address]] Args<Other...> other;
                constexpr Args() {}
                template <class T, class... V>
                constexpr Args(T&& one, Args<V...>&& v)
                    : one(std::forward<T>(one)), other(std::forward<Args<V...>>(v)) {}
                template <size_t index, size_t current = 0>
                constexpr decltype(auto) get() {
                    if constexpr (index == current) {
                        return one.get();
                    }
                    else {
                        return other.template get<index, current + 1>();
                    }
                }
                static constexpr auto size_ = sizeof...(Other) + 1;

                constexpr size_t size() const {
                    return size_;
                }

                constexpr auto invoke_sequence() {
                    return std::make_index_sequence<size_>{};
                }

               private:
                template <class Fn, size_t... idx>
                constexpr decltype(auto) invoke_fn_impl(Fn&& fn, std::index_sequence<idx...>) {
                    return fn(get<idx>()...);
                }

               public:
                template <class Fn>
                constexpr decltype(auto) invoke(Fn&& fn) {
                    return invoke_fn_impl(fn, invoke_sequence());
                }
            };

            template <class One, class... Arg>
            Args(One, Args<Arg...>) -> Args<One, Arg...>;

            template <class T>
            struct arg_cast {
                static constexpr bool is_array = std::is_array_v<std::remove_reference_t<T>>;
                static constexpr bool is_not_pointer = !std::is_pointer_v<std::remove_reference_t<T>>;
                using Target = std::remove_extent_t<std::remove_reference_t<T>>*;
                using Type = std::conditional_t<is_array && is_not_pointer,
                                                Target,
                                                T>;
                static constexpr bool is_const = std::is_const_v<std::remove_reference_t<Type>>;

                using Const = std::conditional_t<is_not_pointer && is_const,
                                                 std::remove_cvref_t<Type>,
                                                 Type>;
                using type = Const;
            };

            template <class T>
            using arg_t = typename arg_cast<T>::type;

            constexpr auto make_arg() {
                return Args<>{};
            }

            template <class One, class... Arg>
            constexpr auto make_arg(One&& one, Arg&&... arg) {
                return Args<arg_t<One>, arg_t<Arg>...>{std::forward<One>(one), make_arg(std::forward<Arg>(arg)...)};
            }

            template <class Ret, class Fn, class... Arg>
            struct FuncRecord {
                Fn fn;
                Args<Arg...> args;
                Args<Ret> retobj;

                constexpr FuncRecord(FuncRecord&& rec)
                    : fn(std::forward<Fn>(rec.fn)),
                      args(std::forward<Args<Arg...>>(rec.args)),
                      retobj(std::forward<Args<Ret>>(rec.retobj)) {}

                constexpr FuncRecord(Fn&& in, Arg&&... arg)
                    : fn(std::forward<Fn>(in)),
                      args(make_arg(std::forward<Arg>(arg)...)) {
                }

                constexpr void execute() {
                    if constexpr (std::is_same_v<Ret, void>) {
                        args.invoke(fn);
                    }
                    else {
                        retobj = make_arg(args.invoke(fn));
                    }
                }

                constexpr void operator()() {
                    execute();
                }
            };

            template <class Fn, class... Arg>
            using invoke_res = decltype(std::declval<Args<arg_t<Arg>...>>().invoke(std::declval<Fn>()));

            template <class Fn, class... Arg>
            constexpr auto make_funcrecord(Fn&& fn, Arg&&... arg) {
                return FuncRecord<invoke_res<Fn, Arg...>, std::decay_t<Fn>, arg_t<Arg>...>{
                    std::forward<Fn>(fn),
                    std::forward<Arg>(arg)...,
                };
            }

        }  // namespace light
    }      // namespace async
}  // namespace utils
