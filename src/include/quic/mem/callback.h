/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// callback - flexible callback
#pragma once
#include <type_traits>

namespace utils {
    namespace quic {
        namespace mem {
            template <class T, class Req>
            concept void_or_type = std::is_void_v<Req> || std::is_same_v<T, Req>;

            template <class T, class Ret, class... Args>
            concept callable = requires(T t) {
                { t(std::declval<Args>()...) } -> void_or_type<Ret>;
            };

            template <class T>
            struct RetDefault {
                using Ret = T;
                static constexpr T default_v() noexcept {
                    if constexpr (std::is_void_v<Ret>) {
                        return (void)0;
                    }
                    else {
                        return Ret{};
                    }
                }
            };

            template <class T, class Exception>
            struct RetExcept {
                using Ret = T;
                static T default_v() {
                    if constexpr (std::is_default_constructible_v<T>) {
                        return RetDefault<T>::default_v();
                    }
                    else {
                        throw Exception();
                    }
                }
            };

            template <class Rettraits>
            auto select_call(auto&& callback) {
                using Ret = typename Rettraits::Ret;
                if constexpr (callable<decltype(callback), Ret>) {
                    return callback();
                }
                else {
                    callback();
                    return Rettraits::default_v();
                }
            }

            struct nocontext_t {};

            template <class Ctx, class RetTraits, class... Args>
            struct CBCore {
                using Ret = typename RetTraits::Ret;
                static constexpr bool no_context = std::is_same_v<Ctx, nocontext_t>;
                using callback_t = std::conditional_t<
                    no_context,
                    Ret (*)(void*, Args...),
                    Ret (*)(void*, Ctx, Args...)>;
                void* func_ctx = nullptr;
                callback_t cb = nullptr;

                template <class... CallArg>
                Ret operator()(CallArg&&... args) {
                    return cb ? cb(func_ctx, std::forward<CallArg>(args)...) : RetTraits::default_v();
                }

                constexpr CBCore() = default;
                constexpr CBCore(std::nullptr_t)
                    : CBCore() {}
                constexpr CBCore(callback_t cb, void* ctx)
                    : cb(cb), func_ctx(ctx) {}
                template <class Ptr>
                static Ret callback_core(void* c, Ctx ctx, Args... args) {
                    auto& call = (*static_cast<Ptr>(c));
                    using T = decltype(call);
                    if constexpr (!no_context && callable<T, void, Ctx, Args...>) {
                        return select_call<RetTraits>([&] {
                            return call(std::forward<Ctx>(ctx), std::forward<Args>(args)...);
                        });
                    }
                    else if constexpr (callable<T, void, Args...>) {
                        return select_call<RetTraits>([&] {
                            return call(std::forward<Args>(args)...);
                        });
                    }
                    else {
                        return select_call<RetTraits>([&] {
                            return call();
                        });
                    }
                };
                constexpr CBCore(auto&& v) {
                    auto ptr = std::addressof(v);
                    if constexpr (std::is_same_v<decltype(ptr), callback_t>) {
                        cb = ptr;
                        func_ctx = nullptr;
                    }
                    else {
                        if constexpr (no_context) {
                            cb = [](void* c, Args... args) {
                                return callback_core<decltype(ptr)>(c, {}, std::forward<Args>(args)...);
                            };
                        }
                        else {
                            cb = callback_core<decltype(ptr)>;
                        }
                        func_ctx = ptr;
                    }
                }

                explicit operator bool() {
                    return cb != nullptr;
                }
            };

            template <class Ctx, class Ret, class... Args>
            using CB = CBCore<Ctx, RetDefault<Ret>, Args...>;

        }  // namespace mem
    }      // namespace quic
}  // namespace utils
