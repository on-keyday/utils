/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// urlencode - url encoding
#pragma once
#include "../../core/sequencer.h"
#include "../../number/parse.h"

namespace utils {
    namespace net {
        namespace urlenc {

            constexpr auto default_noescape() {
                return [](auto) {
                    return false;
                };
            }

            // same filter as same name function in javascript
            // see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
            constexpr auto encodeURIComponent() {
                return [](std::uint8_t c) {
                    return c >= 'A' && c <= 'Z' ||
                           c >= 'a' && c <= 'z' ||
                           c >= '0' && c <= '9' ||
                           c == '_' ||
                           c == '-' || c == '.' ||
                           c == '!' || c == '~' ||
                           c == '*' || c == '\'' ||
                           c == '(' || c == ')';
                };
            }

            // same filter as same name function in javascript
            // see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURI
            constexpr auto encodeURI() {
                return [](std::uint8_t c) {
                    constexpr auto encodeURIComponent_ = encodeURIComponent();
                    return encodeURIComponent_(c) ||
                           c == ';' || c == ',' ||
                           c == '/' || c == '?' ||
                           c == ':' || c == '@' ||
                           c == '&' || c == '=' ||
                           c == '+' || c == '#';
                };
            }

            template <class T, class Out, class F = void (*)(std::uint8_t)>
            constexpr bool encode(Sequencer<T>& seq, Out& out, F&& no_escape = default_noescape(), bool upper = false) {
                while (!seq.eos()) {
                    if (seq.current() < 0 || seq.current() > 0xff) {
                        return false;
                    }
                    if (no_escape(seq.current())) {
                        out.push_back(seq.current());
                    }
                    else {
                        auto n = std::uint8_t(seq.current());
                        out.push_back('%');
                        out.push_back(number::to_num_char((n & 0xf0) >> 4, upper));
                        out.push_back(number::to_num_char((n & 0x0f), upper));
                    }
                    seq.consume();
                }
                return true;
            }

            template <class In, class Out, class F = void (*)(std::uint8_t)>
            constexpr bool encode(In&& in, Out& out, F&& no_escape = default_noescape(), bool upper = false, bool expect_eos = true) {
                auto seq = make_ref_seq(in);
                auto res = encode(seq, out, no_escape, upper);
                if (res && expect_eos) {
                    return seq.eos();
                }
                return res;
            }

            template <class T, class Out>
            constexpr bool decode(Sequencer<T>& seq, Out& out) {
                while (!seq.eos()) {
                    if (seq.current() == '%') {
                        seq.consume();
                        auto n = seq.current();
                        seq.consume();
                        auto s = seq.current();
                        seq.consume();
                        if (n < 0 || n > 0xff || s < 0 || s > 0xff) {
                            return false;
                        }
                        auto v1 = number::number_transform[n];
                        auto v2 = number::number_transform[s];
                        if (v1 < 0x0 || v1 > 0xf || v2 < 0x0 || v2 > 0xf) {
                            return false;
                        }
                        out.push_back(std::uint8_t(v1 << 4 | v2));
                    }
                    else {
                        out.push_back(seq.current());
                        seq.consume();
                    }
                }
                return true;
            }

            template <class In, class Out>
            constexpr bool decode(In&& in, Out& out, bool expect_eos = true) {
                auto seq = make_ref_seq(in);
                auto res = decode(seq, out);
                if (res && expect_eos) {
                    return seq.eos();
                }
                return res;
            }
        }  // namespace urlenc

    }  // namespace net
}  // namespace utils
