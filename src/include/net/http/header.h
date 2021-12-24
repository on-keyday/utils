/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include "../../helper/readutil.h"
#include "../../helper/pushbacker.h"
#include "../../number/number.h"

namespace utils {
    namespace net {
        struct StatusCode {
            std::uint16_t code = 0;
            void push_back(auto v) {
                number::internal::PushBackParserInt<std::uint16_t> tmp;
                tmp.result = code;
                tmp.push_back(v);
                if (!tmp.overflow) {
                }
            }
            operator std::uint16_t() {
                return code;
            }
        };

        template <class String, class T, class Result>
        bool header_parse_common(Sequencer<T>& seq, Result& result) {
            while (true) {
                if (helper::match_eol(seq)) {
                    break;
                }
                String key, value;
                if (!helper::read_while<true>(key, seq, [](auto v) {
                        return v != ':';
                    })) {
                    return false;
                }
                if (!seq.consume_if(':')) {
                    return false;
                }
                helper::read_while(helper::nop, seq, [](auto v) {
                    return v == ' ';
                });
                if (!helper::read_while<true>(value, seq, [](auto v) {
                        return v != '\r' && v != '\n';
                    })) {
                    return false;
                }
                result.emplace(std::move(key), std::move(value));
                if (!helper::match_eol(seq)) {
                    return false;
                }
            }
            return true;
        }

        template <class String, class T, class Result, class Method, class Path, class Version>
        bool request_header(Sequencer<T>& seq, Method& method, Path& path, Version& ver, Result& result) {
            if (!helper::read_while<true>(method, seq, [](auto v) {
                    return v != ' ';
                })) {
                return false;
            }
            if (!seq.consume_if(' ')) {
                return false;
            }
            if (!helper::read_while<true>(path, seq, [](auto v) {
                    return v != ' ';
                })) {
                return false;
            }
            if (!seq.consume_if(' ')) {
                return false;
            }
            if (!helper::read_while<true>(ver, seq, [](auto v) {
                    return v != '\r' && v != '\n';
                })) {
                return false;
            }
            if (!helper::match_eol(seq)) {
                return false;
            }
            return header_parse_common(seq, result);
        }

        template <class String, class T, class Result, class Version, class Status, class Phrase>
        bool response_header(Sequencer<T>& seq, Version& ver, Status& status, Phrase& phrase, Result& result) {
            if (!helper::read_while<true>(ver, seq, [](auto v) {
                    return v != ' ';
                })) {
                return false;
            }
            if (!seq.consume_if(' ')) {
                return false;
            }
            if (!helper::read_n(status, seq, 3, [](auto v) {

                })) {
                return false;
            }
            if (!seq.consume_if(' ')) {
                return false;
            }
            if (!helper::read_while<true>(phrase, seq, [](auto v) {
                    return v != 'r' && v != '\n';
                })) {
                return false;
            }
            if (!helper::match_eol(seq)) {
                return false;
            }
            return header_parse_common<String>(seq, result);
        }
    }  // namespace net
}  // namespace utils
