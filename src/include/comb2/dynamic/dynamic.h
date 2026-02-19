/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <memory>
#include <type_traits>
#include "../internal/opti.h"
#include "comb2/internal/opti.h"
#include "comb2/status.h"
#include <helper/defer.h>
#include <comb2/internal/context_fn.h>
namespace futils::comb2 {
    namespace types {

        template <class I, class C, class R, class A = std::allocator<char>>
        struct TypeErased : opti::MaybeEmptyA<A> {
            using allocator_type = A;

            constexpr TypeErased()
                requires std::is_default_constructible_v<A>
                : opti::MaybeEmptyA<A>(A{}) {}

            constexpr TypeErased(std::allocator_arg_t, const A& a)
                : opti::MaybeEmptyA<A>(a) {}

            template <class T>
            explicit TypeErased(std::allocator_arg_t, const A& a, T&& t)
                : opti::MaybeEmptyA<A>(a) {
                using decay_T = std::decay_t<T>;
                using AllocTraits = std::allocator_traits<A>;
                using AllocRebind = typename AllocTraits::template rebind_alloc<decay_T>;
                using AllocRebindTraits = std::allocator_traits<AllocRebind>;
                AllocRebind alloc(this->useA());
                auto p = AllocRebindTraits::allocate(alloc, 1);
                auto guard = helper::defer([&]() {
                    AllocRebindTraits::deallocate(alloc, p, 1);
                });
                AllocRebindTraits::construct(alloc, p, std::forward<T>(t));
                ptr = p;
                guard.cancel();
                dtor = &dtor_fn<decay_T>;
                fn = &call_fn<decay_T>;
                must_match_err = &call_must_match_err<decay_T>;
            }

            constexpr TypeErased(TypeErased&& other) noexcept
                : opti::MaybeEmptyA<A>(other.useA()), ptr(other.ptr), dtor(other.dtor), fn(other.fn), must_match_err(other.must_match_err) {
                other.ptr = nullptr;
                other.dtor = nullptr;
                other.fn = nullptr;
                other.must_match_err = nullptr;
            }

            constexpr TypeErased& operator=(TypeErased&& other) noexcept {
                if (this != &other) {
                    if (ptr) {
                        auto defer_ = helper::defer([&] {
                            ptr = nullptr;
                            dtor = nullptr;
                            fn = nullptr;
                            must_match_err = nullptr;
                        });
                        dtor(*this, ptr);
                    }
                    if constexpr (std::is_reference_v<decltype(this->useA())>) {
                        this->useA() = std::move(other.useA());
                    }
                    ptr = std::exchange(other.ptr, nullptr);
                    dtor = std::exchange(other.dtor, nullptr);
                    fn = std::exchange(other.fn, nullptr);
                    must_match_err = std::exchange(other.must_match_err, nullptr);
                }
                return *this;
            }

            ~TypeErased() {
                if (ptr) {
                    auto defer_ = helper::defer([&] {
                        ptr = nullptr;
                        dtor = nullptr;
                        fn = nullptr;
                        must_match_err = nullptr;
                    });
                    dtor(*this, ptr);
                }
            }

            constexpr Status operator()(I i, C c, R r) const {
                return fn(ptr, i, c, r);
            }

            constexpr void must_match_error(I i, C c, R r) const {
                return must_match_err(ptr, i, c, r);
            }

            constexpr explicit operator bool() const {
                return ptr != nullptr;
            }

           private:
            void* ptr = nullptr;
            void (*dtor)(TypeErased&, void*) = nullptr;
            Status (*fn)(void*, I, C, R) = nullptr;
            void (*must_match_err)(void*, I, C, R) = nullptr;
            template <class T>
            static void dtor_fn(TypeErased& self, void* p) {
                auto t = static_cast<T*>(p);
                using AllocTraits = std::allocator_traits<A>;
                using AllocRebind = typename AllocTraits::template rebind_alloc<T>;
                using RebindTraits = std::allocator_traits<AllocRebind>;
                decltype(auto) alloc_ = self.useA();
                AllocRebind alloc(alloc_);
                RebindTraits::destroy(alloc, t);
                RebindTraits::deallocate(alloc, t, 1);
            }

            template <class T>
            static Status call_fn(void* p, I i, C c, R r) {
                if (!p) {
                    return Status::fatal;
                }
                auto t = static_cast<T*>(p);
                return (*t)(i, std::forward<C>(c), std::forward<R>(r));
            }
            template <class T>
            static void call_must_match_err(void* p, I& i, C&& c, R&& r) {
                if (!p) {
                    ctxs::context_error(i, c, "null pointer at type erased");
                    return;
                }
                auto t = static_cast<T*>(p);
                t->must_match_error(i, std::forward<C>(c), std::forward<R>(r));
            }
        };
    }  // namespace types

    namespace ops {
        template <class I, class C, class R, class A = std::allocator<char>, class T>
        constexpr auto type_erased(T&& t) {
            return types::TypeErased<I, C, R, A>{std::allocator_arg, A{}, std::forward<T>(t)};
        }

        template <class I, class C, class R, class A, class T>
        constexpr auto type_erased_with_allocator(A&& a, T&& t) {
            return types::TypeErased<I, C, R, A>{std::allocator_arg, std::forward<A>(a), std::forward<T>(t)};
        }
    }  // namespace ops

}  // namespace futils::comb2
