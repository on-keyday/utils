/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// c++23, std::expected compatible
#pragma once
#include "../core/byte.h"
#include <new>
#include <type_traits>
#include <utility>
#include "template_instance.h"
#include <memory>
#include <initializer_list>
#include <exception>

namespace utils {
    namespace helper::either {
        // some code are copied from https://cpprefjp.github.io/reference/expected/expected.html and it's linked pages
        // which is under CC Attribution 3.0 Unported
        // and code are changed to use in helper
        // for example, adding std:: prefix to use std functions
        // MIT license is maybe compatible to CC
        template <class T, class U>
        struct expected;

        template <class E>
        struct unexpected;
        namespace internal {

            template <class T, class W>
            constexpr bool converts_from_any_cvref =
                std::disjunction_v<std::is_constructible<T, W&>, std::is_convertible<W&, T>,
                                   std::is_constructible<T, W>, std::is_convertible<W, T>,
                                   std::is_constructible<T, const W&>, std::is_convertible<const W&, T>,
                                   std::is_constructible<T, const W>, std::is_convertible<const W, T>>;

            template <class T, class E, class U, class G>
            constexpr bool not_convertible_to_unexpected = converts_from_any_cvref<T, expected<U, G>> == false &&
                                                           std::is_constructible_v<unexpected<E>, expected<U, G>&> == false &&
                                                           std::is_constructible_v<unexpected<E>, expected<U, G>> == false &&
                                                           std::is_constructible_v<unexpected<E>, const expected<U, G>&> == false &&
                                                           std::is_constructible_v<unexpected<E>, const expected<U, G>> == false;

            enum holds : byte {
                holds_T,
                holds_E
            };

            template <class T, class E, class U>
            concept is_valid_T = !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> && !std::is_same_v<expected<T, E>, std::remove_cvref_t<U>> &&
                                 !is_template_instance_of<std::remove_cvref_t<U>, unexpected>;
            template <class F, class... Value>
            struct wrap_void_call {
                using type = std::invoke_result_t<F, Value...>;
            };

            template <class F, class Value>
                requires std::is_void_v<Value>
            struct wrap_void_call<F, Value> {
                using type = std::invoke_result_t<F>;
            };

            template <class F, class... Args>
            using f_result_t = std::remove_cvref_t<typename wrap_void_call<F, Args...>::type>;
            template <class T, class Value>
            concept is_callable = (std::is_void_v<Value> ? std::invocable<T> : std::invocable<T, Value>);

            template <class F, class Value, class E>
            concept is_and_then_monad =
                is_callable<F, Value> &&
                is_template_instance_of<f_result_t<F, Value>, expected> &&
                std::is_same_v<typename f_result_t<F, Value>::error_type, E>;

            template <class F, class Value, class T>
            concept is_or_else_monad =
                is_callable<F, Value> &&
                is_template_instance_of<f_result_t<F, Value>, expected> &&
                std::is_same_v<typename f_result_t<F, Value>::value_type, T>;

            template <class U, class Ivk, class E>
            concept is_transform_monad_impl = is_valid_T<U, E, U> &&
                                              std::is_convertible_v<Ivk, U>;

            template <class F, class Value, class E>
            concept is_transform_monad =
                is_callable<F, Value> &&
                is_transform_monad_impl<
                    std::remove_cvref_t<typename wrap_void_call<F, Value>::type>,
                    typename wrap_void_call<F, Value>::type,
                    E>;

            template <class U, class Ivk>
            concept is_transform_error_monad_impl = std::is_constructible_v<
                U,
                Ivk>;

            template <class F, class Value>
            concept is_transform_error_monad =
                is_callable<F, Value> &&
                is_transform_error_monad_impl<
                    std::remove_cvref_t<typename wrap_void_call<F, Value>::type>,
                    typename wrap_void_call<F, Value>::type>;

            template <class T>
            concept has_error_buffer_type = requires(T t) {
                typename T::error_buffer_type;
                { t.error(std::declval<typename T::error_buffer_type&>()) };
                { std::declval<const typename T::error_buffer_type&>().c_str() } -> std::convertible_to<const char*>;
            };

            template <class T>
            concept has_c_str = requires(T t) {
                { t.c_str() } -> std::convertible_to<const char*>;
            };

        }  // namespace internal

        struct unexpect_t {
            explicit unexpect_t() = default;
        };

        inline constexpr unexpect_t unexpect{};

        template <class E>
        struct bad_expected_access;

        template <>
        struct bad_expected_access<void> : std::exception {
            bad_expected_access() = default;
        };

        template <class E>
        struct bad_expected_access : public bad_expected_access<void> {
           private:
            E e;

           public:
            explicit bad_expected_access(E e)
                : e(std::move(e)) {}
            bad_expected_access(const bad_expected_access& e)
                : e(e.e){};
            bad_expected_access(bad_expected_access&& e)
                : e(std::move(e.e)){};

            const char* what() const noexcept override {
                return "bad expected access when error state";
            }

            constexpr const E& error() const& noexcept {
                return e;
            }

            constexpr E& error() & noexcept {
                return e;
            }

            constexpr const E&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr E&& error() && noexcept {
                return std::move(e);
            }
        };

        template <class E>
            requires internal::has_error_buffer_type<E>
        struct bad_expected_access<E> : public bad_expected_access<void> {
           private:
            E e;
            using B = typename E::error_buffer_type;
            B b;

           public:
            explicit bad_expected_access(E e)
                : e(std::move(e)) {
                this->e.error(b);
            }
            bad_expected_access(const bad_expected_access& e)
                : e(e.e), b(e.b){};
            bad_expected_access(bad_expected_access&& e)
                : e(std::move(e.e)), b(std::move(e.b)){};

            const char* what() const noexcept override {
                return b.c_str();
            }

            constexpr const E& error() const& noexcept {
                return e;
            }

            constexpr E& error() & noexcept {
                return e;
            }

            constexpr const E&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr E&& error() && noexcept {
                return std::move(e);
            }
        };

        template <class E>
            requires internal::has_c_str<E>
        struct bad_expected_access<E> : public bad_expected_access<void> {
           private:
            E e;

           public:
            explicit bad_expected_access(E e)
                : e(std::move(e)) {
            }
            bad_expected_access(const bad_expected_access& e)
                : e(e.e){};
            bad_expected_access(bad_expected_access&& e)
                : e(std::move(e.e)){};

            const char* what() const noexcept override {
                return e.c_str();
            }

            constexpr const E& error() const& noexcept {
                return e;
            }

            constexpr E& error() & noexcept {
                return e;
            }

            constexpr const E&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr E&& error() && noexcept {
                return std::move(e);
            }
        };

        template <>
        struct bad_expected_access<const char*> : public bad_expected_access<void> {
           private:
            const char* e;

           public:
            explicit bad_expected_access(const char* e)
                : e(e) {}

            bad_expected_access(const bad_expected_access& e) = default;
            bad_expected_access(bad_expected_access&& e) = default;

            const char* what() const noexcept override {
                return e;
            }

            constexpr const char* const& error() const& noexcept {
                return e;
            }

            constexpr const char*& error() & noexcept {
                return e;
            }

            constexpr const char* const&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr const char*&& error() && noexcept {
                return std::move(e);
            }
        };

        template <class E>
        struct unexpected {
           private:
            template <class U, class G>
            friend struct expected;
            E hold;

           public:
            constexpr unexpected(const unexpected&) = default;
            constexpr unexpected(unexpected&&) = default;

            template <class Err = E>
                requires std::is_constructible_v<E, Err> &&
                         (!std::is_same_v<std::remove_cvref_t<Err>, unexpected>) &&
                         (!std::is_same_v<std::remove_cvref_t<Err>, std::in_place_t>)
            constexpr explicit unexpected(Err&& e)
                : hold(std::forward<Err>(e)) {}

            template <class... Args>
                requires std::is_constructible_v<E, Args...>
            constexpr explicit unexpected(std::in_place_t, Args&&... args)
                : hold(std::forward<Args>(args)...) {}
            template <class U, class... Args>
                requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
            constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> il, Args&&... args)
                : hold(il, std::forward<Args>(args)...) {}

            constexpr const E& error() const& noexcept {
                return hold;
            }

            constexpr E& error() & noexcept {
                return hold;
            }
            constexpr const E&& error() const&& noexcept {
                return std::move(hold);
            }

            constexpr E&& error() && noexcept {
                return std::move(hold);
            }

            template <class E2>
                requires(std::equality_comparable_with<E, E2>)
            friend constexpr bool operator==(const unexpected& x, const unexpected<E2>& y) {
                return x.error() == y.error();
            }
        };

        template <class E>
        unexpected(E) -> unexpected<E>;

        template <class T, class E>
        struct expected {
            using value_type = T;
            using error_type = E;
            using unexpected_type = unexpected<E>;
            template <class U>
            using rebind = expected<U, error_type>;

           private:
            constexpr static size_t larger_size = sizeof(T) > sizeof(E)
                                                      ? sizeof(T)
                                                      : sizeof(E);
            constexpr static size_t larger_align = alignof(T) > alignof(E) ? alignof(T) : alignof(E);

            union {
                alignas(larger_align) byte data[larger_size];
                T t;
                E e;
            };
            bool holds;

            constexpr void set_holds(internal::holds h) {
                holds = h == internal::holds_T;
            }

            constexpr T* construct_T(auto&&... a) {
                auto ptr = std::construct_at(std::addressof(t), std::forward<decltype(a)>(a)...);
                set_holds(internal::holds_T);
                return std::launder(ptr);
            }

            constexpr E* construct_E(auto&&... a) {
                auto ptr = std::construct_at(std::addressof(e), std::forward<decltype(a)>(a)...);
                set_holds(internal::holds_E);
                return std::launder(ptr);
            }

            constexpr void do_construct(auto&& m, auto&& apply) {
                if (m.has_value()) {
                    construct_T(apply(*m));
                }
                else {
                    construct_E(apply(m.error()));
                }
            }

            constexpr void move_construct(auto&& m) {
                do_construct(std::forward<decltype(m)>(m), [](auto& a) -> decltype(auto) {
                    return std::move(a);
                });
            }

            constexpr void copy_construct(const auto& m) {
                do_construct(std::forward<decltype(m)>(m), [](const auto& a) -> decltype(auto) {
                    return a;
                });
            }

            constexpr void destruct() {
                if (has_value()) {
                    std::destroy_at(std::addressof(t));
                }
                else {
                    std::destroy_at(std::addressof(e));
                }
            }

           public:
            constexpr bool has_value() const noexcept {
                return holds == true;
            }

            constexpr expected()
                requires std::is_default_constructible_v<T>
            {
                construct_T();
            }

            template <class... Args>
                requires std::is_nothrow_constructible_v<T, Args...>
            constexpr T& emplace(Args&&... args) {
                destruct();
                return *construct_T(std::forward<Args>(args)...);
            }

            constexpr void swap(expected& rhs) noexcept(std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_swappable_v<T>&& std::is_nothrow_move_constructible_v<E>&& std::is_nothrow_swappable_v<E>)
                requires std::is_swappable_v<T> && std::is_swappable_v<E> &&
                         (std::is_move_constructible_v<T> &&
                          std::is_move_constructible_v<E>) &&
                         (std::is_nothrow_move_constructible_v<T> ||
                          std::is_nothrow_move_constructible_v<E>)
            {
                if (has_value() == rhs.has_value()) {
                    if (has_value()) {
                        std::swap(t, rhs.t);
                    }
                    else {
                        std::swap(e, rhs.e);
                    }
                    return;
                }
                if (!has_value()) {
                    return rhs.swap(*this);
                }
                // copies from https://cpprefjp.github.io/reference/expected/expected/swap.html
                if constexpr (std::is_nothrow_move_constructible_v<E>) {
                    E tmp(std::move(rhs.e));
                    std::destroy_at(std::addressof(rhs.e));
                    try {
                        std::construct_at(std::addressof(rhs.t), std::move(t));
                        std::destroy_at(std::addressof(t));
                        std::construct_at(std::addressof(e), std::move(tmp));
                    } catch (...) {
                        std::construct_at(std::addressof(rhs.e), std::move(tmp));
                        throw;
                    }
                }
                else {
                    T tmp(std::move(t));
                    std::destroy_at(std::addressof(t));
                    try {
                        std::construct_at(std::addressof(e), std::move(rhs.e));
                        std::destroy_at(std::addressof(rhs.e));
                        std::construct_at(std::addressof(rhs.t), std::move(tmp));
                    } catch (...) {
                        std::construct_at(std::addressof(t), std::move(tmp));
                        throw;
                    }
                }
                set_holds(internal::holds_E);
                rhs.set_holds(internal::holds_T);
            }

            template <class U, class... Args>
                requires std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>
            constexpr T& emplace(std::initializer_list<U> il, Args&&... args) {
                destruct();
                return *construct_T(il, std::forward<Args>(args)...);
            }

            constexpr expected(const expected& rhs)
                requires std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
            {
                copy_construct(rhs);
            }

            constexpr expected(expected&& rhs)
                requires std::is_move_constructible_v<T> && std::is_move_constructible_v<E>
            {
                move_construct(std::move(rhs));
            }

            template <class U, class G>
                requires std::is_constructible_v<T, const U&> && std::is_constructible_v<E, const G&> &&
                         internal::not_convertible_to_unexpected<T, E, U, G>
            constexpr explicit(!std::is_convertible_v<const U&, T> || !std::is_convertible_v<const G&, E>)
                expected(const expected<U, G>& rhs) {
                copy_construct(rhs);
            }

            template <class U, class G>
                requires std::is_constructible_v<T, U> && std::is_constructible_v<E, G> &&
                         internal::not_convertible_to_unexpected<T, E, U, G>
            constexpr explicit(!std::is_convertible_v<U, T> || !std::is_convertible_v<G, E>)
                expected(expected<U, G>&& rhs) {
                move_construct(std::move(rhs));
            }

            template <class U = T>
                requires internal::is_valid_T<T, E, T> && std::is_constructible_v<T, U>
            constexpr explicit(!std::is_convertible_v<U, T>) expected(U&& v) {
                construct_T(std::forward<U>(v));
            }

            template <class G>
                requires std::is_constructible_v<E, const G&>
            constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const unexpected<G>& e) {
                construct_E(e.hold);
            }

            template <class G>
                requires std::is_constructible_v<E, G>
            constexpr explicit(!std::is_convertible_v<G, E>) expected(unexpected<G>&& e) {
                construct_E(std::move(e.hold));
            }

            template <class... Args>
                requires std::is_constructible_v<T, Args...>
            constexpr explicit expected(std::in_place_t, Args&&... args) {
                construct_T(std::forward<Args>(args)...);
            }

            template <class U, class... Args>
                requires std::is_constructible_v<T, std::initializer_list<U>&, Args...>
            constexpr explicit expected(std::in_place_t, std::initializer_list<U> il, Args&&... args) {
                construct_T(il, std::forward<Args>(args)...);
            }

            template <class... Args>
                requires std::is_constructible_v<E, Args...>
            constexpr explicit expected(unexpect_t, Args&&... args) {
                construct_E(std::forward<Args>(args)...);
            }

            template <class U, class... Args>
                requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
            constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args) {
                construct_E(il, std::forward<Args>(args)...);
            }

           private:
            // copied from https://cpprefjp.github.io/reference/expected/expected/op_assign.html
            // under Attribution 3.0 Unported (CC BY 3.0)
            template <class V, class U, class... Args>
            constexpr void reinit_expected(V& newval, U& oldval, Args&&... args) {
                if constexpr (std::is_nothrow_constructible_v<V, Args...>) {
                    std::destroy_at(std::addressof(oldval));
                    std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
                }
                else if constexpr (std::is_nothrow_move_constructible_v<V>) {
                    V tmp(std::forward<Args>(args)...);
                    std::destroy_at(std::addressof(oldval));
                    std::construct_at(std::addressof(newval), std::move(tmp));
                }
                else {
                    U tmp(std::move(oldval));
                    std::destroy_at(std::addressof(oldval));
                    try {
                        std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
                    } catch (...) {
                        std::construct_at(std::addressof(oldval), std::move(tmp));
                        throw;
                    }
                }
            }

            constexpr void do_assign(auto&& rhs, auto&& apply) {
                if (has_value() == rhs.has_value()) {
                    if (has_value()) {
                        this->t = apply(rhs.t);
                    }
                    else {
                        this->e = apply(rhs.e);
                    }
                    return;
                }
                if (has_value()) {
                    reinit_expected(e, t, apply(rhs.e));
                    set_holds(internal::holds::holds_E);
                }
                else {
                    reinit_expected(t, e, apply(rhs.t));
                    set_holds(internal::holds::holds_T);
                }
            }

            template <bool has_val>
            constexpr void do_assign_no_expected(auto&& value) {
                if (has_value() == has_val) {
                    if constexpr (has_val) {
                        this->t = std::forward<decltype(value)>(value);
                    }
                    else {
                        this->e = std::forward<decltype(value)>(value);
                    }
                }
                else {
                    if constexpr (has_val) {
                        reinit_expected(t, e, std::forward<decltype(value)>(value));
                        set_holds(internal::holds::holds_T);
                    }
                    else {
                        reinit_expected(e, t, std::forward<decltype(value)>(value));
                        set_holds(internal::holds::holds_E);
                    }
                }
            }

           public:
            constexpr expected& operator=(const expected& rhs)
                requires std::is_copy_constructible_v<T> &&
                         std::is_copy_assignable_v<T> &&
                         std::is_copy_constructible_v<E> &&
                         std::is_copy_assignable_v<E> &&
                         (std::is_nothrow_move_constructible_v<T> ||
                          std::is_nothrow_move_constructible_v<E>)
            {
                if (this == &rhs) {
                    return *this;
                }
                do_assign(rhs, [](const auto& b) -> decltype(auto) { return b; });
                return *this;
            }

            constexpr expected& operator=(expected&& rhs) noexcept(
                std::is_nothrow_move_assignable_v<T>&& std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_move_assignable_v<E>&& std::is_nothrow_move_constructible_v<E>)
                requires std::is_move_constructible_v<T> &&
                         std::is_move_assignable_v<T> &&
                         std::is_move_constructible_v<E> &&
                         std::is_move_assignable_v<E> &&
                         (std::is_nothrow_move_constructible_v<T> ||
                          std::is_nothrow_move_constructible_v<E>)
            {
                if (this == &rhs) {
                    return *this;
                }
                do_assign(rhs, [](auto& b) -> decltype(auto) { return std::move(b); });
                return *this;
            }

            template <class U = T>
                requires(!std::is_same_v<expected, std::remove_cvref_t<U>>) &&
                        std::is_constructible_v<T, U> &&
                        std::is_assignable_v<T&, U> && (!is_template_instance_of<std::remove_cvref_t<U>, unexpected>) &&
                        (std::is_nothrow_constructible_v<T, U> ||
                         std::is_nothrow_move_constructible_v<T> ||
                         std::is_nothrow_move_constructible_v<E>)
            constexpr expected& operator=(U&& v) {
                do_assign_no_expected<true>(std::forward<decltype(v)>(v));
                return *this;
            }

            template <class G>
                requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&> &&
                         (std::is_nothrow_constructible_v<E, const G&> ||
                          std::is_nothrow_move_constructible_v<T> ||
                          std::is_nothrow_move_constructible_v<E>)
            constexpr expected& operator=(const unexpected<G>& e) {
                do_assign_no_expected<false>(std::forward<const G&>(e.hold));
                return *this;
            }

            template <class G>
                requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G> &&
                         (std::is_nothrow_constructible_v<E, G> ||
                          std::is_nothrow_move_constructible_v<T> ||
                          std::is_nothrow_move_constructible_v<E>)
            constexpr expected& operator=(unexpected<G>&& e) {
                do_assign_no_expected<false>(std::forward<G>(e.hold));
                return *this;
            }

            constexpr ~expected() {
                destruct();
            }

           private:
#define must_copy(has_val)                  \
    if (has_val != has_value()) {           \
        throw bad_expected_access(error()); \
    }

#define must_move(has_val)                             \
    if (has_val != has_value()) {                      \
        throw bad_expected_access(std::move(error())); \
    }

           public:
            constexpr const T& value() const& {
                must_copy(true);
                return t;
            }
            constexpr T& value() & {
                must_copy(true);
                return t;
            }
            constexpr const T&& value() const&& {
                must_move(true);
                return std::move(t);
            }
            constexpr T&& value() && {
                must_move(true);
                return std::move(t);
            }

            constexpr const E& error() const& noexcept {
                return e;
            }

            constexpr E& error() & noexcept {
                return e;
            }
            constexpr const E&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr E&& error() && noexcept {
                return std::move(e);
            }

            // extended methods
            constexpr T* value_ptr() noexcept {
                if (has_value()) {
                    return std::addressof(t);
                }
                return nullptr;
            }

            // extended methods
            constexpr const T* value_ptr() const noexcept {
                if (has_value()) {
                    return std::addressof(t);
                }
                return nullptr;
            }

            // extended methods
            constexpr E* error_ptr() noexcept {
                if (!has_value()) {
                    return std::addressof(e);
                }
                return nullptr;
            }

            // extended methods
            constexpr const E* error_ptr() const noexcept {
                if (!has_value()) {
                    return std::addressof(e);
                }
                return nullptr;
            }

            constexpr const T* operator->() const noexcept {
                if (has_value()) {
                    return std::addressof(t);
                }
                return nullptr;
            }

            constexpr T* operator->() noexcept {
                if (has_value()) {
                    return std::addressof(t);
                }
                return nullptr;
            }

            constexpr const T& operator*() const& noexcept {
                return t;
            }
            constexpr T& operator*() & noexcept {
                return t;
            }
            constexpr T&& operator*() && noexcept {
                return std::move(t);
            }

            constexpr const T&& operator*() const&& noexcept {
                return std::move(t);
            }

            constexpr explicit operator bool() const noexcept {
                return has_value();
            }

            template <class U>
                requires std::is_copy_constructible_v<T> && std::is_convertible_v<U, T>
            constexpr T value_or(U&& v) const& {
                return has_value() ? **this : static_cast<T>(std::forward<U>(v));
            }

            template <class U>
                requires std::is_move_constructible_v<T> && std::is_convertible_v<U, T>
            constexpr T value_or(U&& v) && {
                return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
            }

            template <class G = E>
                requires std::is_copy_constructible_v<E> && std::is_convertible_v<G, E>
            constexpr E error_or(G&& e) const& {
                return has_value() ? std::forward<G>(e) : error();
            }

            template <class G = E>
                requires std::is_move_constructible_v<E> && std::is_convertible_v<G, E>
            constexpr E error_or(G&& e) && {
                return has_value() ? std::forward<G>(e) : std::move(error());
            }

#define do_and_then(apply)                                       \
    using U = internal::f_result_t<F, decltype(apply(value()))>; \
    if (has_value()) {                                           \
        return std::invoke(std::forward<F>(f), apply(value()));  \
    }                                                            \
    else {                                                       \
        return U(unexpect, apply(error()));                      \
    }

#define do_or_else(apply)                                        \
    using G = internal::f_result_t<F, decltype(apply(error()))>; \
    if (has_value()) {                                           \
        return G(std::in_place, apply(value()));                 \
    }                                                            \
    else {                                                       \
        return std::invoke(std::forward<F>(f), apply(error()));  \
    }

#define do_transform(apply)                                                     \
    using U = internal::f_result_t<F, decltype(apply(value()))>;                \
    if (!has_value()) {                                                         \
        return expected<U, E>(unexpect, apply(error()));                        \
    }                                                                           \
    if constexpr (std::is_void_v<U>) {                                          \
        std::invoke(std::forward<F>(f), apply(value()));                        \
        return expected<U, E>();                                                \
    }                                                                           \
    else {                                                                      \
        return expected<U, E>(std::invoke(std::forward<F>(f), apply(value()))); \
    }

#define do_transform_error(apply)                                                         \
    using G = internal::f_result_t<F, decltype(apply(error()))>;                          \
    if (has_value()) {                                                                    \
        return expected<T, G>(std::in_place, apply(value()));                             \
    }                                                                                     \
    else {                                                                                \
        return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), apply(error()))); \
    }

#define do_nothing(x) x

            template <class F>
                constexpr auto and_then(F&& f) &
                    requires std::is_copy_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(do_nothing);
            }

            template <class F>
            constexpr auto and_then(F&& f) const&
                requires std::is_copy_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(do_nothing);
            }

            template <class F>
                constexpr auto and_then(F&& f) &&
                    requires std::is_move_constructible_v<E> && internal::is_and_then_monad<F, decltype(std::move(value())), E>
            {
                do_and_then(std::move);
            }

            template <class F>
            constexpr auto and_then(F&& f) const&&
                requires std::is_move_constructible_v<E> && internal::is_and_then_monad<F, decltype(std::move(value())), E>
            {
                do_and_then(std::move);
            }

            template <class F>
                constexpr auto or_else(F&& f) &
                    requires std::is_copy_constructible_v<T> && internal::is_or_else_monad<F, decltype(error()), T>
            {
                do_or_else(do_nothing);
            }

            template <class F>
            constexpr auto or_else(F&& f) const&
                requires std::is_copy_constructible_v<T> && internal::is_or_else_monad<F, decltype(error()), T>
            {
                do_or_else(do_nothing);
            }

            template <class F>
                constexpr auto or_else(F&& f) &&
                    requires std::is_move_constructible_v<T> && internal::is_or_else_monad<F, decltype(std::move(error())), T>
            {
                do_or_else(std::move);
            }

            template <class F>
            constexpr auto or_else(F&& f) const&&
                requires std::is_move_constructible_v<T> && internal::is_or_else_monad<F, decltype(std::move(error())), T>
            {
                do_or_else(std::move);
            }

            template <class F>

                constexpr auto transform(F&& f) &
                requires(std::is_copy_constructible_v<E>&& internal::is_transform_monad<F, decltype(value()), E>) {
                    do_transform(do_nothing);
                }

                template <class F>
                constexpr auto transform(F&& f) const&
                    requires std::is_copy_constructible_v<E> && internal::is_transform_monad<F, decltype(value()), E>

            {
                do_transform(do_nothing);
            }

            template <class F>
                constexpr auto transform(F&& f) &&
                    requires std::is_move_constructible_v<E> && internal::is_transform_monad<F, decltype(std::move(value())), E>
            {
                do_transform(std::move);
            }

            template <class F>
            constexpr auto transform(F&& f) const&&
                requires std::is_move_constructible_v<E> && internal::is_transform_monad<F, decltype(std::move(value())), E>
            {
                do_transform(std::move);
            }

            template <class F>

                constexpr auto transform_error(F&& f) &
                    requires std::is_copy_constructible_v<T> && internal::is_transform_error_monad<F, decltype(error())>
            {
                do_transform_error(do_nothing);
            }

            template <class F>
            constexpr auto transform_error(F&& f) const&
                requires std::is_copy_constructible_v<T> && internal::is_transform_error_monad<F, decltype(error())>
            {
                do_transform_error(do_nothing);
            }

            template <class F>
                constexpr auto transform_error(F&& f) &&
                    requires std::is_move_constructible_v<T> && internal::is_transform_error_monad<F, decltype(std::move(error()))>
            {
                do_transform_error(std::move);
            }

            template <class F>
            constexpr auto transform_error(F&& f) const&&
                requires std::is_move_constructible_v<T> && internal::is_transform_error_monad<F, decltype(std::move(error()))>
            {
                do_transform_error(std::move);
            }

#undef do_and_then
#undef do_or_else
#undef do_transform
#undef do_transform_error
            // do_nothing,must_copy,must_move is used by expected<void,E>

            template <class T2, class E2>
                requires(!std::is_void_v<T2> &&
                         std::equality_comparable_with<T, T2> &&
                         std::equality_comparable_with<E, E2>)
            friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y) {
                if (x.has_value() != y.has_value()) {
                    return false;
                }
                if (x.has_value()) {
                    return *x == *y;
                }
                else {
                    return x.error() == y.error();
                }
            }

            template <class T2>

            friend constexpr bool operator==(const expected& x, const T2& v) {
                return x.has_value() && static_cast<bool>(*x == v);
            }

            template <class E2>
            friend constexpr bool operator==(const expected& x, const unexpected<E2>& v) {
                return !x.has_value() && static_cast<bool>(x.error() == v.error());
            }
        };  // namespace helper::either

        template <class T, class E>
            requires std::is_void_v<T>
        struct expected<T, E> {
            using value_type = T;
            using error_type = E;
            using unexpected_type = unexpected<E>;
            template <class U>
            using rebind = expected<U, error_type>;

           private:
            union {
                alignas(alignof(E)) byte data[sizeof(E)];
                E e;
            };
            bool holds;

            constexpr void set_holds(internal::holds h) {
                holds = h == internal::holds_T;
            }

            constexpr void construct_T() {
                set_holds(internal::holds_T);
            }

            constexpr E* construct_E(auto&&... a) {
                auto ptr = std::construct_at(std::addressof(e), std::forward<decltype(a)>(a)...);
                set_holds(internal::holds_E);
                return std::launder(ptr);
            }

            constexpr void do_construct(auto&& m, auto&& apply) {
                if (m.has_value()) {
                    construct_T();
                }
                else {
                    construct_E(apply(m.error()));
                }
            }

            constexpr void move_construct(auto&& m) {
                do_construct(std::forward<decltype(m)>(m), [](auto& a) -> decltype(auto) {
                    return std::move(a);
                });
            }

            constexpr void copy_construct(const auto& m) {
                do_construct(std::forward<decltype(m)>(m), [](const auto& a) -> decltype(auto) {
                    return a;
                });
            }

            constexpr void destruct() {
                if (!has_value()) {
                    std::destroy_at(std::addressof(e));
                }
            }

           public:
            constexpr bool has_value() const {
                return holds == true;
            }

            constexpr expected() {
                construct_T();
            }

            constexpr void emplace() noexcept {
                destruct();
                construct_T();
            }

            constexpr void swap(expected& rhs) noexcept(std::is_nothrow_move_constructible_v<E>&& std::is_nothrow_swappable_v<E>)
                requires std::is_swappable_v<E> && std::is_move_constructible_v<E>
            {
                if (has_value() == rhs.has_value()) {
                    if (has_value()) {
                        return;  // nothing to do
                    }
                    std::swap(e, rhs.e);
                    return;
                }
                if (!has_value()) {
                    return rhs.swap(*this);
                }
                std::construct_at(std::addressof(e), std::move(rhs.e));
                std::destroy_at(std::addressof(rhs.e));
                set_holds(internal::holds_E);
                rhs.set_holds(internal::holds_T);
            }

            constexpr expected(const expected& rhs)
                requires std::is_copy_constructible_v<E>
            {
                copy_construct(rhs);
            }

            constexpr expected(expected&& rhs)
                requires std::is_move_constructible_v<T>
            {
                move_construct(std::move(rhs));
            }

            template <class U, class G>
                requires std::is_void_v<U> && std::is_constructible_v<E, const G&> &&
                         internal::not_convertible_to_unexpected<void, E, U, G>
            constexpr explicit(!std::is_convertible_v<const G&, E>)
                expected(const expected<U, G>& rhs) {
                copy_construct(rhs);
            }

            template <class U, class G>
                requires std::is_void_v<U> && std::is_constructible_v<E, G> &&
                         internal::not_convertible_to_unexpected<void, E, U, G>
            constexpr explicit(!std::is_convertible_v<G, E>)
                expected(expected<U, G>&& rhs) {
                move_construct(std::move(rhs));
            }

            template <class G>
                requires std::is_constructible_v<E, const G&>
            constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const unexpected<G>& e) {
                construct_E(e.hold);
            }

            template <class G>
                requires std::is_constructible_v<E, G>
            constexpr explicit(!std::is_convertible_v<G, E>) expected(unexpected<G>&& e) {
                construct_E(std::move(e.hold));
            }

            constexpr explicit expected(std::in_place_t) {
                construct_T();
            }

            template <class... Args>
                requires std::is_constructible_v<E, Args...>
            constexpr explicit expected(unexpect_t, Args&&... args) {
                construct_E(std::forward<Args>(args)...);
            }

            template <class U, class... Args>
                requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
            constexpr explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args) {
                construct_E(il, std::forward<Args>(args)...);
            }

           private:
            constexpr void do_assign(auto&& rhs, auto&& apply) {
                if (has_value() == rhs.has_value()) {
                    if (has_value()) {
                        // nothing to do
                    }
                    else {
                        this->e = apply(rhs.e);
                    }
                    return;
                }
                if (has_value()) {
                    construct_E(apply(rhs.e));
                }
                else {
                    std::destroy_at(std::addressof(e));
                    construct_T();
                }
            }

            constexpr void do_assign_no_expected(auto&& value) {
                if (has_value()) {
                    construct_E(std::forward<decltype(value)>(value));
                }
                else {
                    this->e = std::forward<decltype(value)>(value);
                }
            }

           public:
            constexpr expected& operator=(const expected& rhs)
                requires std::is_copy_constructible_v<E> &&
                         std::is_copy_assignable_v<E>
            {
                if (this == &rhs) {
                    return *this;
                }
                do_assign(rhs, [](const auto& b) -> decltype(auto) { return b; });
                return *this;
            }

            constexpr expected& operator=(expected&& rhs) noexcept(
                std::is_nothrow_move_assignable_v<E>&& std::is_nothrow_move_constructible_v<E>)
                requires std::is_move_constructible_v<E> &&
                         std::is_move_assignable_v<E>
            {
                if (this == &rhs) {
                    return *this;
                }
                do_assign(rhs, [](auto& b) -> decltype(auto) { return std::move(b); });
                return *this;
            }

            template <class G>
                requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>
            constexpr expected& operator=(const unexpected<G>& e) {
                do_assign_no_expected(std::forward<const G&>(e.hold));
                return *this;
            }

            template <class G>
                requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
            constexpr expected& operator=(unexpected<G>&& e) {
                do_assign_no_expected(std::forward<G>(e.hold));
                return *this;
            }

           private:
           public:
            constexpr ~expected() {
                destruct();
            }

            constexpr void value() const& {
                must_copy(true);
            }

            constexpr void value() && {
                must_move(true);
            }

            constexpr const E& error() const& noexcept {
                return e;
            }

            constexpr E& error() & noexcept {
                return e;
            }
            constexpr const E&& error() const&& noexcept {
                return std::move(e);
            }

            constexpr E&& error() && noexcept {
                return std::move(e);
            }

            // extended methods
            constexpr const E* error_ptr() const noexcept {
                if (!has_value()) {
                    return std::addressof(e);
                }
                return nullptr;
            }

            // extended methods
            constexpr E* error_ptr() noexcept {
                if (!has_value()) {
                    return std::addressof(e);
                }
                return nullptr;
            }

            constexpr void operator*() const noexcept {
                /*nop*/
            }

            constexpr explicit operator bool() const noexcept {
                return has_value();
            }

            template <class G = E>
                requires std::is_copy_constructible_v<E> && std::is_convertible_v<G, E>
            constexpr E error_or(G&& e) const& {
                return has_value() ? std::forward<G>(e) : error();
            }

            template <class G = E>
                requires std::is_move_constructible_v<E> && std::is_convertible_v<G, E>
            constexpr E error_or(G&& e) && {
                return has_value() ? std::forward<G>(e) : std::move(error());
            }

#define do_and_then(apply)                      \
    using U = internal::f_result_t<F>;          \
    if (has_value()) {                          \
        return std::invoke(std::forward<F>(f)); \
    }                                           \
    else {                                      \
        return U(unexpect, apply(error()));     \
    }

#define do_or_else(apply)                                        \
    using G = internal::f_result_t<F, decltype(apply(error()))>; \
    if (has_value()) {                                           \
        return G();                                              \
    }                                                            \
    else {                                                       \
        return std::invoke(std::forward<F>(f), apply(error()));  \
    }

#define do_transform(apply)                                     \
    using U = internal::f_result_t<F>;                          \
    if (!has_value()) {                                         \
        return expected<U, E>(unexpect, apply(error()));        \
    }                                                           \
    if constexpr (std::is_void_v<U>) {                          \
        std::invoke(std::forward<F>(f));                        \
        return expected<U, E>();                                \
    }                                                           \
    else {                                                      \
        return expected<U, E>(std::invoke(std::forward<F>(f))); \
    }

#define do_transform_error(apply)                                                         \
    using G = internal::f_result_t<F, decltype(apply(error()))>;                          \
    if (has_value()) {                                                                    \
        return expected<T, G>();                                                          \
    }                                                                                     \
    else {                                                                                \
        return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), apply(error()))); \
    }

            template <class F>
                constexpr auto and_then(F&& f) &
                    requires std::is_copy_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(do_nothing);
            }

            template <class F>
            constexpr auto and_then(F&& f) const&
                requires std::is_copy_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(do_nothing);
            }

            template <class F>
                constexpr auto and_then(F&& f) &&
                    requires std::is_move_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(std::move);
            }

            template <class F>
            constexpr auto and_then(F&& f) const&&
                requires std::is_move_constructible_v<E> && internal::is_and_then_monad<F, decltype(value()), E>
            {
                do_and_then(std::move);
            }

            template <class F>
                constexpr auto or_else(F&& f) &
                    requires internal::is_or_else_monad<F, decltype(error()), T>
            {
                do_or_else(do_nothing);
            }

            template <class F>
            constexpr auto or_else(F&& f) const&
                requires internal::is_or_else_monad<F, decltype(error()), T>
            {
                do_or_else(do_nothing);
            }

            template <class F>
                constexpr auto or_else(F&& f) &&
                    requires internal::is_or_else_monad<F, decltype(std::move(error())), T>
            {
                do_or_else(std::move);
            }

            template <class F>
            constexpr auto or_else(F&& f) const&&
                requires internal::is_or_else_monad<F, decltype(std::move(error())), T>
            {
                do_or_else(std::move);
            }

            template <class F>

                constexpr auto transform(F&& f) &
                requires(std::is_copy_constructible_v<E>&& internal::is_transform_monad<F, decltype(value()), E>) {
                    do_transform(do_nothing);
                }

                template <class F>
                constexpr auto transform(F&& f) const&
                    requires std::is_copy_constructible_v<E> && internal::is_transform_monad<F, decltype(value()), E>

            {
                do_transform(do_nothing);
            }

            template <class F>
                constexpr auto transform(F&& f) &&
                    requires std::is_move_constructible_v<E> && internal::is_transform_monad<F, decltype(value()), E>
            {
                do_transform(std::move);
            }

            template <class F>
            constexpr auto transform(F&& f) const&&
                requires std::is_move_constructible_v<E> && internal::is_transform_monad<F, decltype(value()), E>
            {
                do_transform(std::move);
            }

            template <class F>

                constexpr auto transform_error(F&& f) &
                    requires internal::is_transform_error_monad<F, decltype(error())>
            {
                do_transform_error(do_nothing);
            }

            template <class F>
            constexpr auto transform_error(F&& f) const&
                requires internal::is_transform_error_monad<F, decltype(error())>
            {
                do_transform_error(do_nothing);
            }

            template <class F>
                constexpr auto transform_error(F&& f) &&
                    requires internal::is_transform_error_monad<F, decltype(std::move(error()))>
            {
                do_transform_error(std::move);
            }

            template <class F>
            constexpr auto transform_error(F&& f) const&&
                requires internal::is_transform_error_monad<F, decltype(std::move(error()))>
            {
                do_transform_error(std::move);
            }

#undef do_and_then
#undef do_or_else
#undef do_transform
#undef do_transform_error
#undef do_nothing

            template <class T2, class E2>
                requires(std::is_void_v<T2>)
            friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y) {
                if (x.has_value() != y.has_value()) {
                    return false;
                }
                return x.has_value() || x.error() == y.error();
            }

            template <class E2>
            friend constexpr bool operator==(const expected& x, const unexpected<E2>& v) {
                return !x.has_value() && static_cast<bool>(x.error() == v.hold);
            }
        };

    }  // namespace helper::either
}  // namespace utils
