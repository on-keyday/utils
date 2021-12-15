/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// utf_convert - utf convert operator>>
#pragma once

#include "../core/sequencer.h"

#include "../utf/convert.h"

namespace utils {
    namespace ops {
        namespace internal {
            template <class T, bool mask_failure>
            struct UtfFromObj {
                Sequencer<T> seq;

                template <class... Args>
                constexpr UtfFromObj(Args&&... args)
                    : seq(std::forward<Args>(args)...) {}
            };

            template <class T, bool mask_failure, class U>
            constexpr bool operator>>(UtfFromObj<T, mask_failure>&& in, U& out) {
                return utf::convert<mask_failure>(in.seq, out);
            }
        }  // namespace internal

        template <bool mask_failure = false, class T>
        constexpr internal::UtfFromObj<buffer_t<T&>, mask_failure> from(T&& t) {
            return internal::UtfFromObj<buffer_t<T&>, mask_failure>(std::forward<T>(t));
        }

    }  // namespace ops
}  // namespace utils
