/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// utf_stream - read any utf string
#pragma once
#include "stream.h"
#include <helper/view.h>
#include <number/array.h>
#include <utf/convert.h>

namespace utils {
    namespace parser {
        namespace stream {

            struct invalid_utf_error {
                size_t pos;
                size_t input_size;
                char inputs[4];
                void error(PB) {}
                void token(PB) {}
                Error err() {
                    return *this;
                }
                TokenInfo info() {
                    return TokenInfo{
                        .has_err = true,
                        .pos = pos,
                    };
                }
            };

            struct VariableParser {
                Token parse(Input& input) {
                    char target[20];
                    size_t r;
                    while (true) {
                        size_t pos = input.pos();
                        auto ptr = input.peek(target, 20, &r);
                        if (ptr == target && r > 20) {
                            return simpleErrToken{"invalid size"};
                        }
                        auto view = helper::SizedView((const std::uint8_t*)ptr, r);
                        auto seq = make_ref_seq(view);
                        while (seq.remain() >= 8) {
                            auto shift = [&](size_t s) -> std::uint64_t {
                                return view.ptr[seq.rptr + s] << (8 * s);
                            };
                            auto val = shift(0) | shift(1) | shift(2) | shift(3) | shift(4) | shift(5) | shift(6) | shift(7);
                            constexpr auto validate = 0x80'80'80'80'80'80'80'80;
                            constexpr auto validat2 = 0x80'80'80'80'00'00'00'00;
                            if (val & validate) {
                                if (val & validat2) {
                                    break;
                                }
                                seq.consume(4);
                                break;
                            }
                            seq.consume(8);
                        }
                        while (!seq.eos()) {
                            number::Array<1, char32_t> tmp;
                            if (!utf::convert_one(seq, tmp)) {
                                auto c = seq.current();
                                if (utf::is_mask_of<1>(c)) {
                                }
                            }
                        }
                    }
                }
            };

        }  // namespace stream
    }      // namespace parser
}  // namespace utils