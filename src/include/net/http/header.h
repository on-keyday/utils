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
#include "../../number/parse.h"
#include "../../helper/appender.h"

namespace utils {
    namespace net {
        namespace h1header {
            struct StatusCode {
                std::uint16_t code = 0;
                void push_back(auto v) {
                    number::internal::PushBackParserInt<std::uint16_t> tmp;
                    tmp.result = code;
                    tmp.push_back(v);
                    code = tmp.result;
                    if (code >= 1000) {
                        code /= 10;
                    }
                }
                operator std::uint16_t() {
                    return code;
                }
            };

            template <class String, class T, class Result, class Preview>
            bool parse_common(Sequencer<T>& seq, Result& result, Preview&& preview) {
                while (true) {
                    if (helper::match_eol(seq)) {
                        break;
                    }
                    String key, value;
                    if (!helper::read_whilef<true>(key, seq, [](auto v) {
                            return v != ':';
                        })) {
                        return false;
                    }
                    if (!seq.consume_if(':')) {
                        return false;
                    }
                    helper::read_whilef(helper::nop, seq, [](auto v) {
                        return v == ' ';
                    });
                    if (!helper::read_whilef<true>(value, seq, [](auto v) {
                            return v != '\r' && v != '\n';
                        })) {
                        return false;
                    }
                    preview(key, value);
                    result.emplace(std::move(key), std::move(value));
                    if (!helper::match_eol(seq)) {
                        return false;
                    }
                }
                return true;
            }

            template <class String, class T, class Result, class Method, class Path, class Version, class PreView = decltype(helper::no_check())>
            bool parse_request(Sequencer<T>& seq, Method& method, Path& path, Version& ver, Result& result, PreView&& preview = helper::no_check()) {
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
                return parse_common(seq, result, preview);
            }

            template <class String, class T, class Result, class Version, class Status, class Phrase, class PreView = decltype(helper::no_check())>
            bool parse_response(Sequencer<T>& seq, Version& ver, Status& status, Phrase& phrase, Result& result, PreView&& preview = helper::no_check()) {
                if (!helper::read_whilef<true>(ver, seq, [](auto v) {
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
                if (!helper::read_whilef<true>(phrase, seq, [](auto v) {
                        return v != '\r' && v != '\n';
                    })) {
                    return false;
                }
                if (!helper::match_eol(seq)) {
                    return false;
                }
                return parse_common<String>(seq, result, preview);
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
                return helper::is_valid<true>(header, [](auto&& c) {
                    if (!number::is_in_visible_range(c)) {
                        return false;
                    }
                    if (!is_valid_key_char(c)) {
                        return false;
                    }
                    return true;
                });
            }

            template <class String>
            constexpr bool is_valid_value(String&& header) {
                return helper::is_valid<true>(header, [](auto&& c) {
                    return number::is_in_visible_range(c);
                });
            }

            constexpr auto default_validator() {
                return [](auto&& keyval) {
                    return is_valid_key(std::get<0>(keyval)) && is_valid_value(std::get<1>(keyval));
                };
            }

            template <class String, class Header, class Validate = decltype(helper::no_check())>
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
                return true;
            }

            template <class String, class Method, class Path, class Header, class Validate = decltype(helper::no_check()), class Prerender = decltype(helper::no_check())>
            bool render_request(String& str, Method&& method, Path&& path, Header& header, Validate&& validate = helper::no_check(), bool ignore_invalid = false,
                                Prerender&& prerender = helper::no_check()) {
                if (helper::contains(method, " ") || helper::contains(path, " ")) {
                    return false;
                }
                helper::append(str, method);
                str.push_back(' ');
                helper::append(str, path);
                helper::append(str, " HTTP/1.1\r\n");
                prerender(str);
                return render_header_common(str, header, validate, ignore_invalid);
            }

            template <class String, class Status, class Phrase, class Header, class Validate = decltype(helper::no_check()), class Prerender = decltype(helper::no_check())>
            bool render_response(String& str, Status&& status, Phrase&&, Header& header, Validate&& validate = helper::no_check(), bool ignore_invalid = false,
                                 Prerender&& prerender = helper::no_check()) {
                if (status < 100 && status > 999) {
                    return false;
                }
                helper::append(str, " HTTP/1.1\r\n");
                std::uint8_t codebuf[4] = "000";
                codebuf[0] += (status / 100);
                codebuf[1] += (status % 100 / 10);
                codebuf[2] += (status % 100 % 10);
                helper::append(str, status);
                helper::append(str, " ");
                helper::append(str, );
                prerender(str);
                return render_header_common(str, header, validate, ignore_invalid);
            }

        }  // namespace h1header

    }  // namespace net
}  // namespace utils
