/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cast - cast if type is not same
#pragma once
#include "convert.h"
#include "../../helper/sfinae.h"

namespace utils {
    namespace utf {
        namespace internal {

            SFINAE_BLOCK_TU_BEGIN(is_constructible_by, std::declval<T&>() = std::declval<U>())
            template <bool, class Uv, class Fn>
            constexpr static bool cast_fn(Uv&& u, Fn&& fn) {
                fn(u);
                return true;
            }
            SFINAE_BLOCK_TU_ELSE(is_constructible_by)
            template <bool mask_failure, class Uv, class Fn>
            constexpr static bool cast_fn(Uv&& u, Fn&& fn) {
                T v;
                if (!utf::convert<mask_failure>(u, v)) {
                    return false;
                }
                fn(v);
                return true;
            }
            SFINAE_BLOCK_TU_END()

            template <bool constructible, bool mask_failure, class Out, class In, class Fn>
            constexpr bool cast_fn(std::enable_if_t<!constructible && std::is_same_v<Out, std::remove_cvref_t<In>>, In>&& in, Fn&& fn) {
                fn(in);
                return true;
            }

            template <bool constructible, bool mask_failure, class Out, class In, class Fn>
            constexpr bool cast_fn(std::enable_if_t<!constructible && !std::is_same_v<Out, std::remove_cvref_t<In>>, In>&& in, Fn&& fn) {
                Out v;
                if (!utf::convert(in, v, mask_failure ? utf::ConvertMode::unsafe : utf::ConvertMode::none)) {
                    return false;
                }
                fn(v);
                return true;
            }

            template <bool constructible, bool mask_failure, class Out, class In, class Fn>
            constexpr bool cast_fn(std::enable_if_t<constructible, In>&& in, Fn&& fn) {
                return is_constructible_by<Out, std::remove_reference_t<In>>::template cast_fn<mask_failure>(in, fn);
            }

        }  // namespace internal

        template <class Out, bool constructible = false, bool mask_failure = false, class In, class Fn>
        constexpr bool cast_fn(In&& in, Fn&& fn) {
            return internal::cast_fn<constructible, mask_failure, Out, In>(in, fn);
        }
    }  // namespace utf
}  // namespace utils
