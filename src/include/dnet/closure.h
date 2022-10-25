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
                    size_,
                    cloneable_,
                    ptr_,
                    typeid_,
                };

                template <class Ret, class... Args>
                struct Clfn {
                    Ret (*fn)(void*, Args...);
                    std::uintptr_t (*ctrl)(void*, ClCtrl);
                };

                template <class T, class Ret, class... Args>
                struct ClfnImpl : Clfn<Ret, Args...> {
                    T real_fn;
                    static Ret fn_(void* p, Args... args) {
                        if constexpr (std::is_same_v<Ret, void>) {
                            (void)static_cast<ClfnImpl*>(p)->real_fn(std::forward<decltype(args)>(args)...);
                        }
                        else {
                            return static_cast<ClfnImpl*>(p)->real_fn(std::forward<decltype(args)>(args)...);
                        }
                    }
                    static std::uintptr_t ctrl_(void* p, ClCtrl c) {
                        if (c == del_) {
                            free_normal(p, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ClfnImpl)));
                            return 0;
                        }
                        if (c == clone_) {
                            if constexpr (std::is_copy_constructible_v<T>) {
                                auto ptr = alloc_normal(sizeof(ClfnImpl), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(ClfnImpl)));
                                if (!ptr) {
                                    return 0;
                                }
                                auto f = new (ptr) ClfnImpl(std::as_const(real_fn));
                                return std::uintptr_t(f);
                            }
                            return 0;
                        }
                        auto ptr = static_cast<ClfnImpl*>(p);
                        if (c == size_) {
                            return sizeof(ptr->real_fn);
                        }
                        if (c == cloneable_) {
                            return std::is_copy_constructible_v<T>;
                        }
                        if (c == ptr_) {
                            return std::uintptr_t(std::addressof(ptr->real_fn));
                        }
                        if (c == typeid_) {
                            return std::uintptr_t(&typeid(T));
                        }
                        return 0;
                    }

                    template <class... A>
                    ClfnImpl(A&&... args)
                        : real_fn(args...), Clfn<Ret, Args...>(fn_, ctrl_) {}
                };
            }  // namespace internal

            template <class Ret, class... Args>
            struct Closure {
               private:
                internal::Clfn<Ret, Args...>* fn = nullptr;

               public:
                constexpr Closure() = default;
                template <class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, Closure>, int> = 0>
                Closure(Fn&& f) {
                    using Fn_t = std::decay_t<Fn>;
                    using Impl_t = internal::ClfnImpl<Fn_t, Ret, Args...>;
                    auto ptr = alloc_normal(sizeof(Impl_t), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(Impl_t)));
                    if (!ptr) {
                        return;
                    }
                    fn = new (ptr) Impl_t(std::forward<decltype(f)>(f));
                }

                constexpr Closure(Closure&& in)
                    : fn(std::exchange(in.fn, nullptr)) {}

                constexpr auto operator()(auto&&... args) const {
                    return fn->fn(fn, std::forward<decltype(args)>(args)...);
                }

                constexpr size_t size() const {
                    return fn->ctrl(fn, internal::size_);
                }

                constexpr Closure(const Closure& in) {
                    if (in.fn && in.fn->ctrl(in.fn, internal::cloneable_)) {
                        fn = static_cast<internal::Clfn*>(reinterpret_cast<void*>(in.fn->ctrl(in.fn, internal::clone_)));
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
                        fn = static_cast<internal::Clfn*>(reinterpret_cast<void*>(in.fn->ctrl(in.fn, internal::clone_)));
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
            };

        }  // namespace closure
    }      // namespace dnet
}  // namespace utils
