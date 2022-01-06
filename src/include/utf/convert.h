/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// convert - generic function for convert utf
#pragma once

#include "internal/convert_impl.h"

namespace utils {
    namespace utf {

        template <bool mask_failure = false, class T, class U>
        constexpr bool convert_one(T&& in, U& out) {
            Sequencer<buffer_t<T&>> seq(in);
            return internal::convert_impl<mask_failure, buffer_t<T&>, U>(seq, out);
        }

        template <bool mask_failure = false, class T, class U>
        constexpr bool convert_one(Sequencer<T>& in, U& out) {
            return internal::convert_impl<mask_failure, T, U>(in, out);
        }

        template <bool mask_failure = false, class T, class U>
        constexpr bool convert(Sequencer<T>& in, U& out) {
            while (!in.eos()) {
                if (!convert_one<mask_failure>(in, out)) {
                    return false;
                }
            }
            return true;
        }

        template <bool mask_failure = false, class T, class U>
        constexpr bool convert(T&& in, U& out) {
            Sequencer<buffer_t<T&>> seq(in);
            return convert<mask_failure>(seq, out);
        }

        template <class String, bool mask_failure = false, class T>
        constexpr String convert(T&& in) {
            String result;
            convert<mask_failure>(in, result);
            return result;
        }

    }  // namespace utf
}  // namespace utils
