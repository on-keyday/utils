/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
#include <helper/equal.h>
#include "../../space/eol.h"

namespace utils {
    namespace http {
        namespace header {
            struct StatusCode {
                std::uint16_t code = 0;
                void append(auto& v) {
                    for (auto& c : v) {
                        push_back(c);
                    }
                }
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
            // parse_common parse common feilds of both http request and http response
            // Argument
            // seq - source of header
            // result - header map. require result.emplace(key,value) method
            // preview - preview header before emplace() call. preview(key,value)
            // prepare - prepare key and value string before reading header. prepare(key,value)
            template <class String, class T, class Result, class Preview, class Prepare>
            bool parse_common(Sequencer<T>& seq, Result& result, Preview&& preview, Prepare&& prepare) {
                while (true) {
                    if (space::parse_eol<true>(seq)) {
                        break;
                    }
                    String key, value;
                    prepare(key, value);
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
                    if (!space::parse_eol<true>(seq)) {
                        return false;
                    }
                }
                return true;
            }

            template <class T, class Method, class Path, class Version>
            bool parse_request_line(Sequencer<T>& seq, Method& method, Path& path, Version& ver) {
                if (!helper::read_whilef<true>(method, seq, [](auto v) {
                        return v != ' ';
                    })) {
                    return false;
                }
                if (!seq.consume_if(' ')) {
                    return false;
                }
                if (!helper::read_whilef<true>(path, seq, [](auto v) {
                        return v != ' ';
                    })) {
                    return false;
                }
                if (!seq.consume_if(' ')) {
                    return false;
                }
                if (!helper::read_whilef<true>(ver, seq, [](auto v) {
                        return v != '\r' && v != '\n';
                    })) {
                    return false;
                }
                if (!space::parse_eol<true>(seq)) {
                    return false;
                }
                return true;
            }

            template <class String, class T, class Result, class Method, class Path, class Version, class PreView = decltype(helper::no_check2()), class Prepare = decltype(helper::no_check2())>
            bool parse_request(Sequencer<T>& seq, Method& method, Path& path, Version& ver, Result& result, PreView&& preview = helper::no_check2(), Prepare&& prepare = helper::no_check2()) {
                if (!parse_request_line(seq, method, path, ver)) {
                    return false;
                }
                return parse_common<String>(seq, result, preview, prepare);
            }

            template <class T, class Version, class Status, class Phrase>
            bool parse_status_line(Sequencer<T>& seq, Version& ver, Status& status, Phrase& phrase) {
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
                if (!space::parse_eol<true>(seq)) {
                    return false;
                }
                return true;
            }

            template <class String, class T, class Result, class Version, class Status, class Phrase, class PreView = decltype(helper::no_check2()), class Prepare = decltype(helper::no_check2())>
            bool parse_response(Sequencer<T>& seq, Version& ver, Status& status, Phrase& phrase, Result& result, PreView&& preview = helper::no_check2(), Prepare&& prepare = helper::no_check2()) {
                if (!parse_status_line(seq, ver, status, phrase)) {
                    return false;
                }
                return parse_common<String>(seq, result, preview, prepare);
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
                    return number::is_in_visible_range(c) || c == ' ' || c == '\t';
                });
            }

            constexpr auto default_validator() {
                return [](auto&& keyval) {
                    return is_valid_key(get<0>(keyval)) && is_valid_value(get<1>(keyval));
                };
            }

            constexpr auto http2_key_validator() {
                return [](auto&& kv) {
                    return helper::equal(get<0>(kv), ":path") ||
                           helper::equal(get<0>(kv), ":authority") ||
                           helper::equal(get<0>(kv), ":status") ||
                           helper::equal(get<0>(kv), ":scheme") ||
                           helper::equal(get<0>(kv), ":method");
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
                    helper::append(str, get<0>(keyval));
                    helper::append(str, ": ");
                    helper::append(str, get<1>(keyval));
                    helper::append(str, "\r\n");
                }
                helper::append(str, "\r\n");
                return true;
            }
            template <class String, class Method, class Path>
            bool render_request_line(String& str, Method&& method, Path&& path) {
                if (helper::contains(method, " ") || helper::contains(path, " ")) {
                    return false;
                }
                helper::append(str, method);
                str.push_back(' ');
                helper::append(str, path);
                helper::append(str, " HTTP/1.1\r\n");
                return true;
            }

            template <class String, class Method, class Path, class Header, class Validate = decltype(helper::no_check()), class Prerender = decltype(helper::no_check())>
            bool render_request(String& str, Method&& method, Path&& path, Header& header, Validate&& validate = helper::no_check(), bool ignore_invalid = false,
                                Prerender&& prerender = helper::no_check()) {
                if (!render_request_line(str, method, path)) {
                    return false;
                }
                prerender(str);
                return render_header_common(str, header, validate, ignore_invalid);
            }

            template <class String, class Status, class Phrase>
            bool render_status_line(String& str, Status&& status, Phrase&& phrase, int verstr = 1) {
                if (status < 100 && status > 999) {
                    return false;
                }
                if (verstr == 2) {
                    helper::append(str, "HTTP/2.0 ");
                }
                else if (verstr == 0) {
                    helper::append(str, "HTTP/1.0 ");
                }
                else {
                    helper::append(str, "HTTP/1.1 ");
                }
                char codebuf[4] = "000";
                codebuf[0] += (status / 100);
                codebuf[1] += (status % 100 / 10);
                codebuf[2] += (status % 100 % 10);
                helper::append(str, codebuf);
                helper::append(str, " ");
                helper::append(str, phrase);
                helper::append(str, "\r\n");
                return true;
            }

            template <class String, class Status, class Phrase, class Header, class Validate = decltype(helper::no_check()), class Prerender = decltype(helper::no_check())>
            bool render_response(String& str, Status&& status, Phrase&& phrase, Header& header, Validate&& validate = helper::no_check(), bool ignore_invalid = false,
                                 Prerender&& prerender = helper::no_check(), int verstr = 1) {
                if (!render_status_line(str, status, phrase, verstr)) {
                    return false;
                }
                prerender(str);
                return render_header_common(str, header, validate, ignore_invalid);
            }

            void canonical_header(auto&& input, auto&& output) {
                auto seq = make_ref_seq(input);
                bool first = true;
                while (!seq.eos()) {
                    if (first) {
                        output.push_back(helper::to_upper(seq.current()));
                        first = false;
                    }
                    else {
                        output.push_back(helper::to_lower(seq.current()));
                        if (seq.current() == '-') {
                            first = true;
                        }
                    }
                    seq.consume();
                }
            }
        }  // namespace header
    }      // namespace http
}  // namespace utils
