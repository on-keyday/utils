/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include "../../helper/readutil.h"
#include "../../helper/strutil.h"
#include "../../helper/pushbacker.h"
#include "../../number/number.h"
#include "../../helper/appender.h"

namespace utils {
    namespace net {
        namespace header {
            struct StatusCode {
                std::uint16_t code = 0;
                void push_back(auto v) {
                    number::internal::PushBackParserInt<std::uint16_t> tmp;
                    tmp.result = code;
                    tmp.push_back(v);
                    code = tmp.result;
                }
                operator std::uint16_t() {
                    return code;
                }
            };

            template <class String, class T, class Result>
            bool parse_common(Sequencer<T>& seq, Result& result) {
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
            bool parse_request(Sequencer<T>& seq, Method& method, Path& path, Version& ver, Result& result) {
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
                return parse_common(seq, result);
            }

            template <class String, class T, class Result, class Version, class Status, class Phrase>
            bool parse_response(Sequencer<T>& seq, Version& ver, Status& status, Phrase& phrase, Result& result) {
                if (!helper::read_while<true>(ver, seq, [](auto v) {
                        return v != ' ';
                    })) {
                    return false;
                }
                if (!seq.consume_if(' ')) {
                    return false;
                }
                bool f = true;
                if (!helper::read_n(status, seq, 3, [&](auto v) {
                        if (f) {
                            if (v == '0') {
                                return false;
                            }
                            f = false;
                        }
                        return number::is_digit(v);
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

            constexpr bool is_valid_key_char(std::uint8_t c) {
                return number::is_alnum(c) ||
                       // #$%&'*+-.^_`|~!
                       c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
                       c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
                       c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
            }

            template <class String>
            constexpr bool is_valid_key(String&& header) {
                auto seq = make_ref_seq(header);
                if (!helper::read_while<true>(helper::nop, seq, [](auto&& c) {
                        if (!number::is_in_visible_range(c)) {
                            return false;
                        }
                        if (!is_valid_header_char(c)) {
                            return false;
                        }
                        return true;
                    })) {
                    return false;
                }
                return seq.eos();
            }

            template <class String>
            constexpr bool is_valid_value(String&& header) {
                auto seq = make_ref_seq(header);
                if (!helper::read_while<true>(helper::nop, seq, [](auto&& c) {
                        return number::is_in_visible_range(c);
                    })) {
                    return false;
                }
                return seq.eos();
            }

            constexpr auto default_validator() {
                return [](auto&& keyval) {
                    return is_valid_key(std::get<0>(keyval)) && is_valid_value(std::get<1>(keyval));
                };
            }

            template <class String, class Header, class Validate>
            bool render_header_common(String& str, Header&& header, Validate&& validate = helper::no_check(), bool ignore_invalid = false) {
                for (auto&& keyval : header) {
                    if (!validate(keyval)) {
                        if (!ignore_invalid) {
                            return false;
                        }
                        continue;
                    }
                    helper::append(str, std::get<0>(keyval));
                    helper::append(str, ": ");
                    helper::append(str, std::get<1>(keyval));
                    helper::append(str, "\r\n");
                }
                helper::append(str, "\r\n");
            }
        }  // namespace header
    }      // namespace net
}  // namespace utils
