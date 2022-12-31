/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// simple_parse - simple parse method
#pragma once
#include <helper/space.h>
#include "interface.h"
#include <number/prefix.h>
#include <escape/read_string.h>

namespace utils {
    namespace parser {
        namespace expr {
            template <class T>
            bool boolean(Sequencer<T>& seq, bool& v, size_t& pos) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (seq.seek_if("true")) {
                    v = true;
                }
                else if (seq.seek_if("false")) {
                    v = false;
                }
                else {
                    return false;
                }
                if (!space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    return false;
                }
                return pos_.ok();
            }

            template <class T, class Int>
            bool integer(Sequencer<T>& seq, Int& v, size_t& pos, int& pf) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (!number::prefix_integer(seq, v, &pf)) {
                    return false;
                }
                if (!space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    return false;
                }
                return pos_.ok();
            }

            template <class T, class String>
            bool string(Sequencer<T>& seq, String& str, size_t& pos, bool& fatal) {
                auto pos_ = save_and_space(seq);
                if (!escape::read_string(str, seq, escape::ReadFlag::escape)) {
                    if (str.size()) {
                        fatal = true;
                    }
                    return false;
                }
                space::consume_space(seq, true);
                return pos_.ok();
            }

            constexpr auto default_filter() {
                return []<class T>(Sequencer<T>& seq) {
                    auto c = seq.current();
                    return number::is_alnum(c) || c == '_';
                };
            }

            template <class String, class T, class Filter = decltype(default_filter())>
            bool variable(Sequencer<T>& seq, String& name, size_t& pos, Filter&& filter = default_filter()) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (!helper::read_whilef<true>(name, seq, [&](auto&&) {
                        return filter(seq);
                    })) {
                    return false;
                }
                return pos_.ok();
            }
        }  // namespace expr
    }      // namespace parser
}  // namespace utils
