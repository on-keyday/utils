/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

            union any_pointer {
                void (*func)();
                void* ctx;
            };

            struct RettraitsBase {
                template <class T>
                static constexpr bool func_v = std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<T>>>;
                template <class Ptr>
                static constexpr auto& get_callable(any_pointer a) noexcept {
                    if constexpr (func_v<Ptr>) {
                        return *reinterpret_cast<Ptr>(a.func);
                    }
                    else {
                        return *static_cast<Ptr>(a.ctx);
                    }
                }

                static constexpr auto get_address(auto&& a) noexcept {
                    if constexpr (func_v<decltype(a)>) {
                        return a;
                    }
                    else {
                        return std::addressof(a);
                    }
                }

                static constexpr void move(auto& cb, auto from_cb, auto& func_ctx, auto from_ctx) noexcept {
                    cb = from_cb;
                    func_ctx = from_ctx;
                }

                static constexpr void copy(auto& cb, auto from_cb, auto& func_ctx, auto from_ctx) noexcept {
                    cb = from_cb;
                    func_ctx = from_ctx;
                }

                static constexpr void destruct(auto& cb, auto& func_ctx) noexcept {}
            };

            template <class T>
            struct RetDefault : RettraitsBase {
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

            template <class T, T def>
            struct RetDefArg : RettraitsBase {
                using Ret = T;
                static constexpr T default_v() noexcept {
                    if constexpr (std::is_void_v<Ret>) {
                        return (void)0;
                    }
                    else {
                        return def;
                    }
                }
            };

            template <class T, class Exception>
            struct RetExcept : RettraitsBase {
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
                if constexpr (!std::is_void_v<Ret> && callable<decltype(callback), Ret>) {
                    return callback();
                }
                else {
                    callback();
                    return Rettraits::default_v();
                }
            }
            struct default_t {};
            struct nocontext_t {};
            struct lastcontext_t {};
            struct noctxlast_t {};

            template <class T, class Self>
            using reject_self = std::enable_if_t<!std::is_same_v<std::remove_cvref_t<T>, Self>, int>;

            template <class Ctx, class Pos, class RetTraits, class... Args>
            struct CBCore {
                using Ret = typename RetTraits::Ret;
                static constexpr bool no_context = std::is_same_v<Pos, nocontext_t> ||
                                                   std::is_same_v<Pos, noctxlast_t>;
                static constexpr bool last_context = std::is_same_v<Pos, lastcontext_t> ||
                                                     std::is_same_v<Pos, noctxlast_t>;
                using Default = Ret (*)(any_pointer, Ctx, Args...);
                using NoContext = Ret (*)(any_pointer, Args...);
                using LastContext = Ret (*)(Ctx, Args..., any_pointer);
                using NocontextLast = Ret (*)(Args..., any_pointer);
                // INFO(on-keyday): too complex!
                using callback_t = std::conditional_t<
                    no_context && last_context,
                    NocontextLast,
                    std::conditional_t<
                        no_context,
                        NoContext,
                        std::conditional_t<
                            last_context,
                            LastContext,
                            Default>>>;

                RetTraits traits{};
                any_pointer fctx;
                callback_t cb = nullptr;

                template <class... CallArg>
                Ret operator()(CallArg&&... args) {
                    if constexpr (last_context) {
                        return cb ? cb(std::forward<CallArg>(args)..., fctx) : traits.default_v();
                    }
                    else {
                        return cb ? cb(fctx, std::forward<CallArg>(args)...) : traits.default_v();
                    }
                }

                constexpr CBCore() = default;

                constexpr CBCore(std::nullptr_t)
                    : CBCore() {}
                constexpr CBCore(callback_t cb, void* ctx)
                    : cb(cb) {
                    fctx.ctx = ctx;
                }

                constexpr CBCore(const CBCore& c)
                    : traits(c.traits) {
                    traits.copy(cb, c.cb, fctx, c.fctx);
                }

                constexpr CBCore(CBCore&& c)
                    : traits(std::move(c.traits)) {
                    traits.move(cb, c.cb, fctx, c.fctx);
                }

                constexpr CBCore& operator=(const CBCore& c) {
                    if (this == &c) {
                        return *this;
                    }
                    this->~CBCore();
                    traits = c.traits;
                    traits.copy(cb, c.cb, fctx, c.fctx);
                    return *this;
                }

                constexpr CBCore& operator=(CBCore&& c) {
                    if (this == &c) {
                        return *this;
                    }
                    this->~CBCore();
                    traits = std::move(c.traits);
                    traits.move(cb, c.cb, fctx, c.fctx);
                    return *this;
                }

               private:
                template <class Ptr>
                static Ret callback_core(any_pointer c, Ctx ctx, Args... args) {
                    auto& call = RetTraits::template get_callable<Ptr>(c);
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
                }

                template <class F>
                void core_init(F&& f) {
                    auto ptr = traits.get_address(f);
                    if constexpr (std::is_same_v<decltype(ptr), callback_t>) {
                        cb = ptr;
                        fctx.ctx = nullptr;
                    }
                    else {
                        if constexpr (no_context && last_context) {
                            cb = [](Args... args, any_pointer c) {
                                return callback_core<decltype(ptr)>(c, {}, std::forward<Args>(args)...);
                            };
                        }
                        else if constexpr (last_context) {
                            cb = [](Ctx ctx, Args... args, any_pointer c) {
                                return callback_core<decltype(ptr)>(c, std::forward<Ctx>(ctx), std::forward<Args>(args)...);
                            };
                        }
                        else if constexpr (no_context) {
                            cb = [](any_pointer c, Args... args) {
                                return callback_core<decltype(ptr)>(c, {}, std::forward<Args>(args)...);
                            };
                        }
                        else {
                            cb = callback_core<decltype(ptr)>;
                        }
                        if constexpr (std::is_function_v<std::remove_pointer_t<decltype(ptr)>>) {
                            fctx.func = reinterpret_cast<void (*)()>(ptr);  // hack warning
                        }
                        else {
                            fctx.ctx = ptr;
                        }
                    }
                }

               public:
                template <class F, reject_self<F, CBCore> = 0>
                constexpr CBCore(RetTraits&& traits, F&& f)
                    : traits(std::move(traits)) {
                    core_init(std::forward<F>(f));
                }

                template <class F, reject_self<F, CBCore> = 0>
                constexpr CBCore(F&& v) {
                    core_init(std::forward<F>(v));
                }

                explicit operator bool() {
                    return cb != nullptr;
                }

                ~CBCore() {
                    traits.destruct(cb, fctx);
                }
            };

            template <class Ctx, class Ret, class... Args>
            using CB = CBCore<Ctx, default_t, RetDefault<Ret>, Args...>;
            template <class Ret, class... Args>
            using CBN = CBCore<nocontext_t, nocontext_t, RetDefault<Ret>, Args...>;

            template <class Ret, class... Args>
            using OSSLCB = CBCore<noctxlast_t, noctxlast_t, RetDefault<Ret>, Args...>;

            template <class Ret, Ret def, class... Args>
            using CBDef = CBCore<nocontext_t, nocontext_t, RetDefArg<Ret, def>, Args...>;

        }  // namespace mem
    }      // namespace quic
}  // namespace utils
