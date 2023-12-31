/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "expected.h"
#include "template_instance.h"

namespace utils::helper::either {
#define apply_none(value) value
#define apply_move(value) std::move(value)
#define apply_rval(...) __VA_ARGS__&&
#define apply_lval(...) __VA_ARGS__&
#define apply_const(...) const __VA_ARGS__

    namespace internal {
        template <class R>
        auto call_transform_or_and_then(auto&& e, auto&& f) {
            if constexpr (is_template_instance_of<R, expected>) {
                return std::forward<decltype(e)>(e).and_then(f);
            }
            else {
                return std::forward<decltype(e)>(e).transform(f);
            }
        }
        template <class R>
        auto call_transform_error_or_or_else(auto&& e, auto&& f) {
            if constexpr (is_template_instance_of<R, expected>) {
                return std::forward<decltype(e)>(e).or_else(f);
            }
            else {
                return std::forward<decltype(e)>(e).transform_error(f);
            }
        }
    }  // namespace internal

#define define_op(apply_value, apply_ref, apply_const)                                                      \
    template <class T, class E, class F>                                                                    \
    constexpr auto operator&(apply_const(apply_ref(expected<T, E>)) e, F&& f)                               \
        ->decltype(internal::call_transform_or_and_then<internal::f_result_t<F, decltype(*e)>>(             \
            std::forward<decltype(e)>(e), f)) {                                                             \
        return internal::call_transform_or_and_then<internal::f_result_t<F, decltype(*e)>>(                 \
            std::forward<decltype(e)>(e), f);                                                               \
    }                                                                                                       \
                                                                                                            \
    template <class T, class E, class F>                                                                    \
    constexpr auto operator|(apply_const(apply_ref(expected<T, E>)) e, F&& f)                               \
        ->decltype(internal::call_transform_error_or_or_else<internal::f_result_t<F, decltype(e.error())>>( \
            std::forward<decltype(e)>(e), f)) {                                                             \
        return internal::call_transform_error_or_or_else<internal::f_result_t<F, decltype(e.error())>>(     \
            std::forward<decltype(e)>(e), f);                                                               \
    }

    define_op(apply_none, apply_lval, apply_none);
    define_op(apply_move, apply_rval, apply_none);
    define_op(apply_none, apply_lval, apply_const);
    define_op(apply_move, apply_rval, apply_const);

#undef apply_none
#undef apply_move
#undef apply_rval
#undef apply_lval
#undef apply_const

}  // namespace utils::helper::either
