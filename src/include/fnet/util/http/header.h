/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header - parse http1 header
#pragma once

#include <strutil/readutil.h>
#include <strutil/strutil.h>
#include <helper/pushbacker.h>
#include <number/parse.h>
#include <strutil/append.h>
#include <strutil/equal.h>
#include <strutil/eol.h>
#include <wrap/light/enum.h>
#include <number/array.h>

namespace futils::http::header {

    struct StatusCode {
        std::uint16_t code = 0;
        void append(auto& v) {
            for (auto& c : v) {
                push_back(c);
            }
        }
        constexpr void push_back(auto v) {
            number::internal::PushBackParserInt<std::uint16_t> tmp;
            tmp.result = code;
            tmp.push_back(v);
            code = tmp.result;
            if (code >= 1000) {
                code /= 10;
            }
        }

        constexpr operator std::uint16_t() const {
            return code;
        }
    };

    struct Range {
        size_t start = 0;
        size_t end = 0;
    };

    struct FieldRange {
        Range key;
        Range value;
    };

    enum class HeaderError {
        none,
        invalid_header,
        invalid_header_key,
        not_colon,
        invalid_header_value,
        validation_error,
        not_end_of_line,

        invalid_method,
        invalid_path,
        invalid_version,
        invalid_status_code,
        invalid_reason_phrase,
        not_space,
    };

    constexpr const char* to_string(HeaderError e) {
        switch (e) {
            case HeaderError::none:
                return "none";
            case HeaderError::invalid_header:
                return "invalid_header";
            case HeaderError::invalid_header_key:
                return "invalid_header_key";
            case HeaderError::not_colon:
                return "not_colon";
            case HeaderError::invalid_header_value:
                return "invalid_header_value";
            case HeaderError::validation_error:
                return "validation_error";
            case HeaderError::not_end_of_line:
                return "not_end_of_line";
            case HeaderError::invalid_method:
                return "invalid_method";
            case HeaderError::invalid_path:
                return "invalid_path";
            case HeaderError::invalid_version:
                return "invalid_version";
            case HeaderError::invalid_status_code:
                return "invalid_status_code";
            case HeaderError::invalid_reason_phrase:
                return "invalid_reason_phrase";
            case HeaderError::not_space:
                return "not_space";
        }
        return "unknown";
    }

    using HeaderErr = wrap::EnumWrap<HeaderError, HeaderError::none, HeaderError::invalid_header>;

    template <class F, class... A>
    constexpr HeaderErr call_and_convert_to_HeaderError(F&& f, A&&... a) {
        if constexpr (std::is_convertible_v<std::invoke_result_t<F, A...>, HeaderError>) {
            return f(std::forward<A>(a)...);
        }
        else if constexpr (std::is_convertible_v<std::invoke_result_t<F, A...>, bool>) {
            if (!f(std::forward<A>(a)...)) {
                return HeaderError::validation_error;
            }
            return HeaderError::none;
        }
        else {
            f(std::forward<A>(a)...);
            return HeaderError::none;
        }
    }

    template <class H, class K, class V>
    constexpr auto apply_call_or_emplace(H&& h, K&& key, V&& value) {
        if constexpr (std::invocable<decltype(h), decltype(std::move(key)), decltype(std::move(value))>) {
            return h(std::move(key), std::move(value));
        }
        else {
            h.emplace(std::move(key), std::move(value));
        }
    }

    template <class H, class F>
    constexpr auto apply_call_or_iter(H&& h, F&& f) {
        if constexpr (std::invocable<decltype(h), decltype(std::move(f))>) {
            return h(std::move(f));
        }
        else {
            for (auto&& kv : h) {
                f(get<0>(kv), get<1>(kv));
            }
        }
    }

    template <class String, class F>
    constexpr auto range_to_string(F&& f) {
        return [=](auto& seq, FieldRange range) {
            String key{}, value{};
            seq.rptr = range.key.start;
            strutil::read_n(key, seq, range.key.end - range.key.start);
            seq.rptr = range.value.start;
            strutil::read_n(value, seq, range.value.end - range.value.start);
            return apply_call_or_emplace(f, std::move(key), std::move(value));
        };
    }

    // parse_common parse common feilds of both http request and http response
    // Argument
    // seq - source of header
    // result - header result function. it should be like this:
    //          HeaderErr(Sequencer<T>& seq, FieldRange range)
    template <class T, class Result>
    constexpr HeaderErr parse_common(Sequencer<T>& seq, Result&& result) {
        while (true) {
            if (strutil::parse_eol<true>(seq)) {
                break;  // empty line; end of header
            }
            FieldRange range;
            range.key.start = seq.rptr;
            if (!strutil::read_whilef<true>(helper::nop, seq, [](auto v) {
                    return v != ':' && v != '\r' && v != '\n';
                })) {
                return HeaderError::invalid_header_key;
            }
            range.key.end = seq.rptr;
            if (!seq.consume_if(':')) {
                return HeaderError::not_colon;
            }
            // skip space
            // OWS = *( SP / HTAB )
            strutil::read_whilef(helper::nop, seq, [](auto v) {
                return v == ' ' || v == '\t';
            });
            range.value.start = seq.rptr;
            if (!strutil::read_whilef<true>(helper::nop, seq, [](auto v) {
                    return v != '\r' && v != '\n';
                })) {
                return HeaderError::invalid_header_value;
            }
            range.value.end = seq.rptr;
            if (!strutil::parse_eol<true>(seq)) {
                return HeaderError::not_end_of_line;
            }
            auto save = seq.rptr;
            if (auto err = call_and_convert_to_HeaderError(result, seq, range); !err) {
                return err;
            }
            seq.rptr = save;  // restore
        }
        return HeaderError::none;
    }

    constexpr auto not_space_or_line = [](auto v) {
        return v != ' ' && v != '\r' && v != '\n';
    };

    constexpr auto not_line = [](auto v) {
        return v != '\r' && v != '\n';
    };

    template <class T, class Method, class Path, class Version>
    constexpr HeaderErr parse_request_line(Sequencer<T>& seq, Method&& method, Path&& path, Version&& version) {
        if (!strutil::read_whilef<true>(method, seq, not_space_or_line)) {
            return HeaderError::invalid_method;
        }
        if (!seq.consume_if(' ')) {
            return HeaderError::not_space;
        }
        if (!strutil::read_whilef<true>(path, seq, not_space_or_line)) {
            return HeaderError::invalid_path;
        }
        if (!seq.consume_if(' ')) {
            return HeaderError::not_space;
        }
        if (!strutil::read_whilef<true>(version, seq, not_line)) {
            return HeaderError::invalid_version;
        }
        if (!strutil::parse_eol<true>(seq)) {
            return HeaderError::not_end_of_line;
        }
        return HeaderError::none;
    }

    template <class T, class Result, class Method, class Path, class Version>
    constexpr HeaderErr parse_request(Sequencer<T>& seq, Method&& method, Path&& path, Version&& version, Result&& result) {
        if (auto err = parse_request_line(seq, method, path, version); !err) {
            return err;
        }
        return parse_common(seq, result);
    }

    template <class T, class Version, class Status, class Phrase>
    constexpr HeaderErr parse_status_line(Sequencer<T>& seq, Version&& version, Status&& status, Phrase&& phrase) {
        if (!strutil::read_whilef<true>(version, seq, not_space_or_line)) {
            return HeaderError::invalid_version;
        }
        if (!seq.consume_if(' ')) {
            return HeaderError::not_space;
        }
        bool first = true;
        if (!strutil::read_n(status, seq, 3, [&](auto v) {
                if (first) {
                    if (v == '0') {
                        return false;
                    }
                    first = false;
                }
                return number::is_digit(v);
            })) {
            return HeaderError::invalid_status_code;
        }
        if (!seq.consume_if(' ')) {
            return HeaderError::not_space;
        }
        // reason phrase is optional so allow empty
        // see https://www.rfc-editor.org/rfc/rfc9112.html#section-4-4
        if (!strutil::read_whilef<false>(phrase, seq, not_line)) {
            return HeaderError::invalid_reason_phrase;
        }
        if (!strutil::parse_eol<true>(seq)) {
            return HeaderError::not_end_of_line;
        }
        return true;
    }

    template <class T, class Result, class Version, class Status, class Phrase>
    constexpr HeaderErr parse_response(Sequencer<T>& seq, Version&& version, Status&& status, Phrase&& phrase, Result&& result) {
        if (auto err = parse_status_line(seq, version, status, phrase); !err) {
            return err;
        }
        return parse_common(seq, result);
    }

    constexpr bool is_valid_key_char(std::uint8_t c) {
        return number::is_alnum(c) ||
               // #$%&'*+-.^_`|~!
               c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
               c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
               c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
    }

    template <class String>
    constexpr bool is_valid_key(String&& key, bool allow_empty = false) {
        return strutil::validate(key, allow_empty, [](auto&& c) {
            if (!number::is_non_space_ascii(c)) {
                return false;
            }
            if (!is_valid_key_char(c)) {
                return false;
            }
            return true;
        });
    }

    template <class String>
    constexpr bool is_valid_value(String&& value, bool allow_empty = false) {
        return strutil::validate(value, allow_empty, [](auto&& c) {
            return number::is_non_space_ascii(c) || c == ' ' || c == '\t';
        });
    }

    constexpr auto default_validator(bool key_allow_empty = false, bool value_allow_empty = false) {
        return [=](auto&& key, auto&& value) {
            return is_valid_key(key, key_allow_empty) && is_valid_value(value, value_allow_empty);
        };
    }

    constexpr bool is_http2_pseudo_key(auto&& key) {
        return strutil::equal(key, ":path") ||
               strutil::equal(key, ":authority") ||
               strutil::equal(key, ":status") ||
               strutil::equal(key, ":scheme") ||
               strutil::equal(key, ":method");
    }

    constexpr auto http2_validator(bool key_allow_empty = false, bool value_allow_empty = false) {
        return [=](auto&& key, auto&& value) {
            return (is_http2_pseudo_key(key) || is_valid_key(key, key_allow_empty)) && is_valid_value(value, value_allow_empty);
        };
    }

    template <class String, class Header, class Validate = decltype(strutil::no_check())>
    constexpr HeaderErr render_header_common(String& str, Header&& header, Validate&& validate = strutil::no_check(), bool ignore_invalid = false) {
        HeaderError validation_error = HeaderError::none;
        auto header_add = [&](auto&& key, auto&& value) -> HeaderErr {
            if (validation_error != HeaderError::none) {
                return validation_error;
            }
            if (!validate(key, value)) {
                if (!ignore_invalid) {
                    validation_error = HeaderError::validation_error;
                }
                return HeaderError::validation_error;
            }
            strutil::append(str, key);
            strutil::append(str, ": ");
            strutil::append(str, value);
            strutil::append(str, "\r\n");
            return HeaderError::none;
        };
        if (auto err = call_and_convert_to_HeaderError(
                [&] {
                    return apply_call_or_iter(header, header_add);
                });
            !err) {
            return err;
        }
        if (validation_error != HeaderError::none) {
            return validation_error;
        }
        strutil::append(str, "\r\n");
        return HeaderError::none;
    }

    template <class String, class Method, class Path>
    constexpr HeaderErr render_request_line(String& str, Method&& method, Path&& path, const char* version_str = "HTTP/1.1") {
        if (strutil::contains(method, " ") ||
            strutil::contains(method, "\r") ||
            strutil::contains(method, "\n")) {
            return HeaderError::invalid_method;
        }
        if (strutil::contains(path, " ") ||
            strutil::contains(path, "\r") ||
            strutil::contains(path, "\n")) {
            return HeaderError::invalid_path;
        }
        // version_str is trusted
        strutil::append(str, method);
        str.push_back(' ');
        strutil::append(str, path);
        str.push_back(' ');
        strutil::append(str, version_str);
        strutil::append(str, "\r\n");
        return HeaderError::none;
    }

    template <class String, class Method, class Path, class Header, class Validate = decltype(strutil::no_check())>
    constexpr HeaderErr render_request(String& str, Method&& method, Path&& path, Header& header, Validate&& validate = strutil::no_check(), bool ignore_invalid = false, const char* version_str = "HTTP/1.1") {
        if (auto err = render_request_line(str, method, path, version_str); !err) {
            return err;
        }
        return render_header_common(str, header, validate, ignore_invalid);
    }

    template <class String, class Status, class Phrase>
    constexpr HeaderErr render_status_line(String& str, Status&& status, Phrase&& phrase, const char* version_str = "HTTP/1.1") {
        auto status_ = int(status);
        if (status_ < 100 && status_ > 599) {
            return HeaderError::invalid_status_code;
        }
        // phrase and version_str is trusted
        strutil::append(str, version_str);
        str.push_back(' ');
        char code[4] = "000";
        code[0] += (status_ / 100);
        code[1] += (status_ % 100 / 10);
        code[2] += (status_ % 100 % 10);
        strutil::append(str, code);
        strutil::append(str, " ");
        strutil::append(str, phrase);
        strutil::append(str, "\r\n");
        return HeaderError::none;
    }

    template <class String, class Status, class Phrase, class Header, class Validate = decltype(strutil::no_check2())>
    constexpr HeaderErr render_response(String& str, Status&& status, Phrase&& phrase, Header& header, Validate&& validate = strutil::no_check(), bool ignore_invalid = false,
                                        const char* version_str = "HTTP/1.1") {
        if (auto err = render_status_line(str, status, phrase, version_str); !err) {
            return err;
        }
        return render_header_common(str, header, validate, ignore_invalid);
    }

    void canonical_header(auto&& input, auto&& output) {
        auto seq = make_ref_seq(input);
        bool first = true;
        while (!seq.eos()) {
            if (first) {
                output.push_back(strutil::to_upper(seq.current()));
                first = false;
            }
            else {
                output.push_back(strutil::to_lower(seq.current()));
                if (seq.current() == '-') {
                    first = true;
                }
            }
            seq.consume();
        }
    }

    namespace test {

        constexpr bool test_http_parse() {
            auto test_request = [&](auto expected_status, auto expect_method, auto expect_path, auto expect_version, auto&& header_list, auto text) {
                auto seq = make_ref_seq(text);
                number::Array<char, 20, true> method{}, path{}, version{};
                size_t index = 0;
                auto err = parse_request(
                    seq, method, path, version,
                    range_to_string<decltype(method)>([&](auto&& key, auto&& value) {
                        if (header_list.size() > index) {
                            if (!strutil::equal(key, header_list[index].first)) {
                                [](auto index) {
                                    throw "key not equal";
                                }(index);
                            }
                            if (!strutil::equal(value, header_list[index].second)) {
                                [](auto index) {
                                    throw "value not equal";
                                }(index);
                            }
                            index++;
                        }
                        else {
                            throw "header_list size not equal";
                        }
                    }));
                if (err != expected_status) {
                    [](auto err) {
                        throw err;
                    }(to_string(err));
                }
                if (err != HeaderError::none) {
                    return true;
                }
                if (!strutil::equal(method, expect_method)) {
                    throw "method not equal";
                }
                if (!strutil::equal(path, expect_path)) {
                    throw "path not equal";
                }
                if (!strutil::equal(version, expect_version)) {
                    throw "version not equal";
                }
                return true;
            };
            auto test_response = [&](auto expected_status, auto expect_version, auto expect_status, auto expect_phrase, auto&& header_list, auto text) {
                auto seq = make_ref_seq(text);
                number::Array<char, 20, true> version{}, phrase{};
                StatusCode status;
                size_t index = 0;
                auto err = parse_response(
                    seq, version, status, phrase,
                    range_to_string<decltype(version)>([&](auto&& key, auto&& value) {
                        if (header_list.size() > index) {
                            if (!strutil::equal(key, header_list[index].first)) {
                                [](auto index, auto actual, auto expect) {
                                    throw "key not equal";
                                }(index, key, header_list[index].first);
                            }
                            if (!strutil::equal(value, header_list[index].second)) {
                                [](auto index, auto actual, auto expect) {
                                    throw "value not equal";
                                }(index, value, header_list[index].second);
                            }
                            index++;
                        }
                        else {
                            throw "header_list size not equal";
                        }
                    }));
                if (err != expected_status) {
                    [](auto err) {
                        throw err;
                    }(to_string(err));
                }
                if (err != HeaderError::none) {
                    return true;
                }
                if (!strutil::equal(version, expect_version)) {
                    throw "version not equal";
                }
                if (status != expect_status) {
                    throw "status not equal";
                }
                if (!strutil::equal(phrase, expect_phrase)) {
                    throw "phrase not equal";
                }
                return true;
            };
            // test request
            // success pattern of request
            using listT = number::Array<std::pair<const char*, const char*>, 10>;
            test_request(HeaderError::none, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\n\r\n");
            test_request(HeaderError::none, "GET", "/", "HTTP/1.1", listT{{{"key", "value"}}, 1}, "GET / HTTP/1.1\r\nkey: value\r\n\r\n");
            test_request(HeaderError::none, "GET", "/", "HTTP/1.1", listT{{{"key", "value"}, {"key2", "value2"}}, 2}, "GET / HTTP/1.1\r\nkey: value\r\nkey2: value2\r\n\r\n");
            // fail pattern of request
            test_request(HeaderError::invalid_method, "", "/", "HTTP/1.1", listT{}, " / HTTP/1.1\r\n\r\n");
            test_request(HeaderError::invalid_path, "GET", " ", "HTTP/1.1", listT{}, "GET  HTTP/1.1\r\n\r\n");
            // test_request(HeaderError::invalid_version, "GET", "/", "HTTP/1.2", listT{}, "GET / HTTP/1.2\r\n\r\n");
            test_request(HeaderError::not_colon, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\nkey\r\n\r\n");
            // test_request(HeaderError::invalid_header_key, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\nkey: value\r\nkey2\r\n\r\n");
            test_request(HeaderError::not_end_of_line, "GET", "/", "HTTP/1.1", listT{}, "GET / HTTP/1.1\r\nkey: value");
            // test response
            // success pattern of response
            test_response(HeaderError::none, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\n\r\n");
            test_response(HeaderError::none, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}}, 1}, "HTTP/1.1 200 OK\r\nkey: value\r\n\r\n");
            test_response(HeaderError::none, "HTTP/1.1", 200, "OK", listT{{{"key", "value"}, {"key2", "value2"}}, 2}, "HTTP/1.1 200 OK\r\nkey: value\r\nkey2: value2\r\n\r\n");
            // fail pattern of response
            test_response(HeaderError::invalid_status_code, "HTTP/1.1", 0, "OK", listT{}, "HTTP/1.1 0 OK\r\n\r\n");
            // test_response(HeaderError::invalid_reason_phrase, "HTTP/1.1", 200, "", listT{}, "HTTP/1.1 200 \r\n\r\n");
            test_response(HeaderError::not_colon, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey\r\n\r\n");
            // test_response(HeaderError::invalid_header_key, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey: value\r\nkey2\r\n\r\n");
            test_response(HeaderError::not_end_of_line, "HTTP/1.1", 200, "OK", listT{}, "HTTP/1.1 200 OK\r\nkey: value");
            return true;
        }

        static_assert(test_http_parse());
    }  // namespace test

}  // namespace futils::http::header
