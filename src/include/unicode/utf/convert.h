/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// convert - for api compatibility
#pragma once
#include "do_convert.h"
#include "../../strutil/append_charsize.h"
#include "../../core/sequencer.h"
#include "../../wrap/light/enum.h"

namespace utils {
    namespace unicode::utf {
        namespace internal {
            template <class U, size_t val>
            struct DecideOutLen {
                static constexpr size_t size = val;
            };
            template <class U>
            struct DecideOutLen<U, 0> {
                static constexpr size_t size = strutil::append_size_v<U>;
            };
        }  // namespace internal

        using UTFErr = wrap::EnumWrap<UTFError, UTFError::none, UTFError::utf_unknown>;

        template <size_t insize = 0, size_t outsize = 0, class T, class U>
        constexpr UTFErr convert_one(Sequencer<T>& input, U&& output, bool same_size_as_copy = false, bool replace = true) {
            constexpr auto in_size = insize == 0 ? sizeof(input.current()) : insize;
            constexpr auto out_size = internal::DecideOutLen<U, outsize>::size;
            if constexpr (in_size == out_size) {
                if (same_size_as_copy) {
                    if (input.eos()) {
                        return UTFError::utf_no_input;
                    }
                    auto d = input.current();
                    if (!unicode::internal::output(output, &d, 1)) {
                        return UTFError::utf_output_limit;
                    }
                    input.consume();
                    return UTFError::none;
                }
            }
            auto [offset, ok] = do_convert_one<in_size, out_size>(input.buf.buffer, input.rptr, input.remain(), output, replace);
            input.rptr += offset;
            return ok;
        }

        template <size_t insize = 0, size_t outsize = 0, class T, class U>
        constexpr UTFErr convert_one(T&& input, U&& output, bool same_size_as_copy = false, bool replace = true) {
            auto seq = make_ref_seq(input);
            return convert_one<insize, outsize>(seq, output, same_size_as_copy, replace);
        }

        template <size_t insize = 0, size_t outsize = 0, class T, class U>
        constexpr UTFErr convert(Sequencer<T>& input, U&& output, bool same_size_as_copy = false, bool replace = true) {
            constexpr auto in_size = insize == 0 ? sizeof(input.current()) : insize;
            constexpr auto out_size = internal::DecideOutLen<U, outsize>::size;
            if constexpr (in_size == out_size) {
                if (same_size_as_copy) {
                    while (!input.eos()) {
                        auto d = input.current();
                        if (!unicode::internal::output(output, &d, 1)) {
                            return UTFError::utf_output_limit;
                        }
                    }
                    return UTFError::none;
                }
            }
            auto [total, next_offset, ok] = do_convert<in_size, out_size>(input.buf.buffer, input.rptr, input.remain(), output, replace);
            input.rptr += total;
            return ok;
        }

        template <size_t insize = 0, size_t outsize = 0, class T, class U>
        constexpr UTFErr convert(T&& input, U&& output, bool same_size_as_copy = false, bool replace = true) {
            auto seq = make_ref_seq(input);
            return convert<insize, outsize>(seq, output, same_size_as_copy, replace);
        }

        template <class Out, size_t insize = 0, size_t outsize = 0>
        constexpr Out convert(auto&& in, bool same_size_as_copy = false, bool replace = true) {
            Out out;
            convert<insize, outsize>(in, out, same_size_as_copy, replace);
            return out;
        }

        constexpr UTFErr to_utf32(auto&& input, auto&& output, bool same_size_as_copy = false, bool replace = false) {
            struct {
                std::remove_reference_t<decltype(output)>& o;
                constexpr void push_back(std::uint32_t c) {
                    o = c;
                }
            } l{output};
            return convert_one<0, 4>(input, l, same_size_as_copy, replace);
        }

        constexpr UTFErr from_utf32(auto&& input, auto&& output, bool same_size_as_copy = false, bool replace = false) {
            constexpr auto outlen = internal::DecideOutLen<decltype(output), 0>::size;
            if constexpr (outlen == 4) {
                if (same_size_as_copy) {
                    return unicode::internal::output(output, &input, 1) ? UTFError::none : UTFError::utf_output_limit;
                }
            }
            auto [_, ok] = do_convert_one<4, outlen>(&input, 0, 1, output, replace);
            return ok;
        }
    }  // namespace unicode::utf
    namespace utf = unicode::utf;
}  // namespace utils
