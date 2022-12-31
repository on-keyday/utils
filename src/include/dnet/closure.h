/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// closure - closure
#pragma once
#include "dll/allocator.h"
#include <utility>
#include <memory>

namespace utils {
    namespace dnet {
        namespace closure {
            namespace internal {

                enum ClCtrl {
                    clone_,
                    del_,
                    cloneable_,
                    get_ctx_,
                };

                template <class T>
                concept has_ctx = requires(T t) {
                                      { t.ctx };
                                  };

                template <class Ret, class... Args>
                struct Clfn {
                    Ret (*fn)(void*, Args...);
                    std::uintptr_t (*ctrl)(void*, ClCtrl);

                    constexpr Clfn(Ret (*fn)(void*, Args...), std::uintptr_t (*ctrl)(void*, ClCtrl))
                        : fn(fn), ctrl(ctrl) {}
                };

                template <class T, class Impl, bool enable_clone>
                std::uintptr_t ctrl_(void* p, ClCtrl c) {
                    auto ptr = static_cast<Impl*>(p);
                    if (c == del_) {
                        delete_with_global_heap(ptr, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(Impl), alignof(Impl)));
                        return 0;
                    }
                    if (c == clone_) {
                        if constexpr (enable_clone && std::is_copy_constructible_v<T>) {
                            auto ptr = alloc_normal(sizeof(Impl), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(Impl), alignof(Impl)));
                            if (!ptr) {
                                return 0;
                            }
                            auto f = new (ptr) Impl(std::as_const(ptr->real_fn));
                            return std::uintptr_t(f);
                        }
                        return 0;
                    }
                    if (c == cloneable_) {
                        return std::is_copy_constructible_v<T>;
                    }
                    if (c == get_ctx_) {
                        if constexpr (has_ctx<Impl>) {
                            return (std::uintptr_t)std::addressof(ptr->ctx);
                        }
                        return 0;
                    }
                    return 0;
                }

                template <class T, class Ret, class... Args>
                struct ClfnImpl1 : Clfn<Ret, Args...> {
                    T real_fn;
                    static Ret fn_(void* p, Args... args) {
                        if constexpr (std::is_same_v<Ret, void>) {
                            (void)static_cast<ClfnImpl1*>(p)->real_fn(std::forward<decltype(args)>(args)...);
                        }
                        else {
                            return static_cast<ClfnImpl1*>(p)->real_fn(std::forward<decltype(args)>(args)...);
                        }
                    }

                    template <class A>
                    ClfnImpl1(A&& args)
                        : real_fn(std::forward<A>(args)), Clfn<Ret, Args...>(fn_, ctrl_<T, ClfnImpl1, true>) {}
                };

                template <class T, class Ctx, class Ret, class... Args>
                struct ClfnImpl2 : Clfn<Ret, Args...> {
                    T real_fn;
                    Ctx ctx;
                    static Ret fn_(void* p, Args... args) {
                        auto ptr = static_cast<ClfnImpl2*>(p);
                        if constexpr (std::is_same_v<Ret, void>) {
                            (void)ptr->real_fn(ptr->ctx, std::forward<decltype(args)>(args)...);
                        }
                        else {
                            return ptr->real_fn(ptr->ctx, std::forward<decltype(args)>(args)...);
                        }
                    }

                    template <class A>
                    ClfnImpl2(A&& args)
                        : real_fn(std::forward<A>(args)), Clfn<Ret, Args...>(fn_, ctrl_<T, ClfnImpl2, false>) {}

                    template <class A, class B>
                    ClfnImpl2(A&& args, B&& ctx)
                        : real_fn(std::forward<A>(args)), ctx(std::forward<B>(ctx)), Clfn<Ret, Args...>(fn_, ctrl_<T, ClfnImpl2, false>) {}
                };
            }  // namespace internal

            // Closure is heap allocated function callback mechanism
            // alive until destructor call
            template <class Ret, class... Args>
            struct Closure {
               private:
                using Fnc = internal::Clfn<Ret, Args...>;
                Fnc* fn = nullptr;
                template <class Impl_t>
                static auto alloc_impl() {
                    return alloc_normal(sizeof(Impl_t), alignof(Impl_t), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(Impl_t), alignof(Impl_t)));
                }

                constexpr Closure(Fnc* ptr)
                    : fn(ptr) {}

               public:
                constexpr Closure() = default;

#define DISABLE_SELF(Fn) std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, Closure>, int> = 0
                template <class Fn, DISABLE_SELF(Fn)>
                Closure(Fn&& f) {
                    *this = Closure::create(std::forward<Fn>(f));
                }

                template <class Fn, DISABLE_SELF(Fn)>
                static Closure create(Fn&& f) {
                    using Fn_t = std::decay_t<Fn>;
                    using Impl_t = internal::ClfnImpl1<Fn_t, Ret, Args...>;
                    auto ptr = alloc_impl<Impl_t>();
                    if (!ptr) {
                        return {};
                    }
                    auto fn = new (ptr) Impl_t(std::forward<Fn>(f));
                    return Closure(static_cast<Fnc*>(fn));
                }

                template <class Ctx, class Fn, DISABLE_SELF(Fn)>
                static Closure create(Fn&& f) {
                    using Fn_t = std::decay_t<Fn>;
                    using Impl_t = internal::ClfnImpl2<Fn_t, Ctx, Ret, Args...>;
                    auto ptr = alloc_impl<Impl_t>();
                    if (!ptr) {
                        return {};
                    }
                    auto fn = new (ptr) Impl_t(std::forward<Fn>(f));
                    return Closure(static_cast<Fnc*>(fn));
                }

                template <class Ctx, class Fn, DISABLE_SELF(Fn)>
                static Closure create(Fn&& f, Ctx&& ctx) {
                    using Fn_t = std::decay_t<Fn>;
                    using Ctx_t = std::decay_t<Ctx>;
                    using Impl_t = internal::ClfnImpl2<Fn_t, Ctx_t, Ret, Args...>;
                    auto ptr = alloc_impl<Impl_t>();
                    if (!ptr) {
                        return {};
                    }
                    auto fn = new (ptr) Impl_t(std::forward<Fn>(f), std::forward<Ctx>(ctx));
                    return Closure(static_cast<Fnc*>(fn));
                }

#undef DISABLE_SELF
                // common functions

                constexpr Closure(Closure&& in)
                    : fn(std::exchange(in.fn, nullptr)) {}

                constexpr auto operator()(auto&&... args) const {
                    return fn->fn(fn, std::forward<decltype(args)>(args)...);
                }

                constexpr Closure(const Closure& in) {
                    if (in.fn && in.fn->ctrl(in.fn, internal::cloneable_)) {
                        fn = static_cast<Fnc*>(reinterpret_cast<void*>(in.fn->ctrl(in.fn, internal::clone_)));
                    }
                }

                constexpr Closure& operator=(Closure&& in) {
                    if (this == &in) {
                        return *this;
                    }
                    this->~Closure();
                    fn = std::exchange(in.fn, nullptr);
                    return *this;
                }

                constexpr Closure& operator=(const Closure& in) {
                    if (this == &in) {
                        return *this;
                    }
                    this->~Closure();
                    fn = nullptr;
                    if (in.fn && in.fn->ctrl(in.fn, internal::cloneable_)) {
                        fn = static_cast<Fnc*>(reinterpret_cast<void*>(in.fn->ctrl(in.fn, internal::clone_)));
                    }
                    return *this;
                }

                constexpr explicit operator bool() const {
                    return fn != nullptr;
                }

                constexpr ~Closure() {
                    if (fn) {
                        fn->ctrl(fn, internal::del_);
                    }
                }

                constexpr void* ctx_ptr() const {
                    return reinterpret_cast<void*>(fn->ctrl(fn, internal::get_ctx_));
                }
            };

            // Callback is reference callback mechanism
            // alive inner function
            template <class Ret, class... Args>
            struct Callback {
               private:
                void* f = nullptr;
                Ret (*cb)(void* f, Args...) = nullptr;

                template <class Fn>
                static Ret cb_(void* f, Args... args) {
                    auto& fn = *static_cast<Fn*>(f);
                    if constexpr (std::is_same_v<Ret, void>) {
                        (void)fn(std::forward<decltype(args)>(args)...);
                    }
                    else {
                        return fn(std::forward<decltype(args)>(args)...);
                    }
                }

               public:
                constexpr Callback() = default;
                constexpr Callback(const Callback&) = default;
                constexpr Callback& operator=(const Callback&) = default;
                template <class F>
                constexpr Callback(F&& v)
                    : f(std::addressof(v)), cb(cb_<std::decay_t<F>>) {}

                constexpr Ret operator()(auto&&... args) const {
                    return cb(f, std::forward<decltype(args)>(args)...);
                }

                constexpr operator bool() const {
                    return cb != nullptr;
                }
            };

        }  // namespace closure
    }      // namespace dnet
}  // namespace utils
