/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// convert - generic function for convert utf
#pragma once

#include "internal/convimpl.h"

namespace utils {
    namespace utf {

        enum class ConvertMode {
            none,
            replace,
            unsafe,
        };

        enum class SameSizeMode {
            none,
            code_point,
        };

        template <size_t insize = 0, size_t outsize = 0, class T>
        constexpr UTFErr convert_one(Sequencer<T>& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            if (cvt == ConvertMode::none) {
                if (same_size == SameSizeMode::none) {
                    return internal::convert_impl2<insize, outsize>(in, out, internal::on_samesize_none(), internal::on_fail_none());
                }
                else if (same_size == SameSizeMode::code_point) {
                    return internal::convert_impl2<insize, outsize>(in, out, internal::on_samesize_code_point(), internal::on_fail_none());
                }
                else {
                    return UTFError::invalid_parameter;
                }
            }
            else if (cvt == ConvertMode::replace) {
                if (same_size == SameSizeMode::none) {
                    return internal::convert_impl2<insize, outsize>(in, out, internal::on_samesize_none(), internal::on_fail_replace());
                }
                else if (same_size == SameSizeMode::code_point) {
                    return internal::convert_impl2<insize, outsize>(in, out, internal::on_samesize_code_point(), internal::on_fail_replace());
                }
                else {
                    return UTFError::invalid_parameter;
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
                    return UTFError::invalid_parameter;
                }
            }
            else {
                return UTFError::invalid_parameter;
            }
        }

        template <size_t insize = 0, size_t outsize = 0>
        constexpr UTFErr convert_one(auto&& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            auto seq = make_ref_seq(in);
            return convert_one<insize, outsize>(seq, out, cvt, same_size);
        }

        template <size_t insize = 0, size_t outsize = 0, class T>
        constexpr UTFErr convert(Sequencer<T>& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            while (!in.eos()) {
                if (auto ok = convert_one<insize, outsize>(in, out, cvt, same_size); !ok) {
                    return ok;
                }
            }
            return UTFError::none;
        }

        template <size_t insize = 0, size_t outsize = 0>
        constexpr UTFErr convert(auto&& in, auto&& out, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            auto seq = make_ref_seq(in);
            return convert<insize, outsize>(seq, out, cvt, same_size);
        }

        template <class Out, size_t insize = 0, size_t outsize = 0>
        constexpr Out convert(auto&& in, ConvertMode cvt = ConvertMode::none, SameSizeMode same_size = SameSizeMode::none) {
            Out out;
            convert<insize, outsize>(in, out, cvt, same_size);
            return out;
        }

    }  // namespace utf
}  // namespace utils
