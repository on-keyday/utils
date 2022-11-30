/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// fmtstring - format string parser
#pragma once
#include "../core/sequencer.h"
#include "../number/parse.h"

namespace utils {
    namespace fmt {
        template <class T>
        constexpr void search_fmt(Sequencer<T>& seq, auto&& normal, auto&& on_fmt) {
            while (!seq.eos()) {
                if (seq.seek_if("%")) {
                    on_fmt(seq);
                    continue;
                }
                normal(seq);
            }
        }

        constexpr auto only_consume() {
            return [](auto& seq) {
                seq.consume();
            };
        }

        constexpr auto push_back_and_consume(auto& pb) {
            return [&](auto& seq) {
                pb.push_back(seq.current());
                pb.consume();
            };
        }

        struct FmtFlag {
            bool number_sign = false;
            bool plus = false;
        };

        constexpr auto fmtprefix(auto detail) {
            return [=](auto& seq) {
                FmtFlag flag;
                while (true) {
                    if (!flag.number_sign && seq.currnt() == '#') {
                        flag.number_sign;
                        seq.consume();
                        continue;
                    }
                    break;
                }
                return detail(seq, flag);
            };
        }
    }  // namespace fmt
}  // namespace utils
