/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// convert - generic function for convert utf
#pragma once

#include "internal/convert_impl.h"
#include "internal/convimpl.h"

namespace utils {
    namespace utf {

        template <bool mask_failure = false, class T, class U>
        [[deprecated]] constexpr bool convert_one_old(T&& in, U& out) {
            Sequencer<buffer_t<T&>> seq(in);
            return internal::convert_impl<mask_failure, buffer_t<T&>, U>(seq, out);
        }

        template <bool mask_failure = false, class T, class U>
        [[deprecated]] constexpr bool convert_one_old(Sequencer<T>& in, U& out) {
            return internal::convert_impl<mask_failure, T, U>(in, out);
        }

        template <bool mask_failure = false, class T, class U>
        [[deprecated]] constexpr bool convert_old(Sequencer<T>& in, U& out) {
            while (!in.eos()) {
                if (!convert_one_old<mask_failure>(in, out)) {
                    return false;
                }
            }
            return true;
        }

        template <bool mask_failure = false, class T, class U>
        [[deprecated]] constexpr bool convert_old(T&& in, U& out) {
            Sequencer<buffer_t<T&>> seq(in);
            return convert_old<mask_failure>(seq, out);
        }

        template <class String, bool mask_failure = false, class T>
        [[deprecated]] constexpr String convert_old(T&& in) {
            String result;
            convert_old<mask_failure>(in, result);
            return result;
        }

        enum class ConvertMode {
            none,
            replace,
            unsafe,
        };

        enum class SameSizeMode {
            none,
            code_point,
        };

        template <class T>
        constexpr UTFErr convert_one(Sequencer<T>& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            if (cvt == ConvertMode::none) {
                if (same_size == SameSizeMode::none) {
                    return internal::convert_impl2(in, out, internal::on_samesize_none(), internal::on_fail_none());
                }
                else if (same_size == SameSizeMode::code_point) {
                    return internal::convert_impl2(in, out, internal::on_samesize_code_point(), internal::on_fail_none());
                }
                else {
                    return UTFError::unknown;
                }
            }
            else if (cvt == ConvertMode::replace) {
                if (same_size == SameSizeMode::none) {
                    return internal::convert_impl2(in, out, internal::on_samesize_none(), internal::on_fail_replace());
                }
                else if (same_size == SameSizeMode::code_point) {
                    return internal::convert_impl2(in, out, internal::on_samesize_code_point(), internal::on_fail_replace());
                }
                else {
                    return UTFError::unknown;
                }
            }
            else if (cvt == ConvertMode::unsafe) {
                if (same_size == SameSizeMode::none) {
                    return internal::convert_impl2(in, out, internal::on_samesize_none(), internal::on_fail_unsafe());
                }
                else if (same_size == SameSizeMode::code_point) {
                    return internal::convert_impl2(in, out, internal::on_samesize_code_point(), internal::on_fail_unsafe());
                }
                else {
                    return UTFError::unknown;
                }
            }
            else {
                return UTFError::unknown;
            }
        }

        constexpr UTFErr convert_one(auto&& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            auto seq = make_ref_seq(in);
            return convert_one(seq, out, cvt, same_size);
        }

        template <class T>
        constexpr UTFErr convert(Sequencer<T>& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            while (!in.eos()) {
                if (auto ok = convert_one(in, out, cvt, same_size); !ok) {
                    return ok;
                }
            }
            return UTFError::none;
        }

        constexpr UTFErr convert(auto&& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            auto seq = make_ref_seq(in);
            return convert(seq, out, cvt, same_size);
        }

        template <class Out>
        constexpr Out convert(auto&& in, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            Out out;
            convert(in, out, cvt, same_size);
            return out;
        }

    }  // namespace utf
}  // namespace utils
