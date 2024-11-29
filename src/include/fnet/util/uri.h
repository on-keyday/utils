/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// uri_parse  uri parser
#pragma once
#include <core/sequencer.h>
#include <strutil/readutil.h>
#include <number/char_range.h>
#include <helper/pushbacker.h>
#include <number/array.h>
#include <strutil/strutil.h>
#include <strutil/equal.h>
#include <strutil/append.h>
#include <view/iovec.h>

namespace futils {
    namespace uri {
        enum class ParseError {
            none,
            scheme_not_found,
            invalid_scheme,
            invalid_path,
            unexpected_double_slash,
            unexpected_colon,
            invalid_ipv6_hostname,
            no_at_mark_bug,
        };

        constexpr const char* err_msg(ParseError err) {
            switch (err) {
                case ParseError::none:
                    return "no error";
                case ParseError::scheme_not_found:
                    return "scheme not found";
                case ParseError::invalid_scheme:
                    return "invalid scheme";
                case ParseError::invalid_path:
                    return "invalid path";
                case ParseError::unexpected_double_slash:
                    return "unexpected double slash";
                case ParseError::unexpected_colon:
                    return "unexpected colon";
                case ParseError::invalid_ipv6_hostname:
                    return "invalid ipv6 hostname";
                case ParseError::no_at_mark_bug:
                    return "no at mark (bug)";
                default:
                    return "unknown";
            }
        }

        constexpr auto colon = ':';
        constexpr auto slash = '/';
        constexpr auto query = '?';
        constexpr auto fragment = '#';

        constexpr auto begin_ipv6 = '[';
        constexpr auto end_ipv6 = ']';
        constexpr auto at_sign = '@';

        // include start, exclude end
        struct Range {
            size_t start;
            size_t end;

            constexpr size_t size() const {
                return end - start;
            }
        };

        struct URIRange {
            Range scheme;     // exclude :
            Range authority;  // exclude //
            Range userinfo;   // exclude @
            Range hostname;   // include [] if ipv6
            Range port;       // exclude :
            Range host;       // include : if port is set
            Range path;       // full path
            Range query;      // include ?
            Range fragment;   // include #
        };

        namespace internal {
            template <class T>
            concept has_substr = requires(T t) {
                { t.substr(size_t(), size_t()) };
            };

            template <class T>
            concept is_sequencer = requires(T t) {
                { t.current() };
                { t.consume() };
                { t.eos() };
                { t.seek_if("") };
                { t.rptr };
            };
        }  // namespace internal

        template <class URI, class Src>
        constexpr void range_to_uri(URI& uri, URIRange& range, Src&& src) {
            auto read_from_seq = [&](auto& seq) {
                auto read = [&](auto& dst, auto& range) {
                    auto start = range.start;
                    auto end = range.end;
                    seq.rptr = start;
                    while (seq.rptr < end) {
                        dst.push_back(seq.current());
                        seq.consume();
                    }
                };
                read(uri.scheme, range.scheme);
                read(uri.authority, range.authority);
                read(uri.userinfo, range.userinfo);
                read(uri.hostname, range.hostname);
                read(uri.port, range.port);
                read(uri.host, range.host);
                read(uri.path, range.path);
                read(uri.query, range.query);
                read(uri.fragment, range.fragment);
            };
            if constexpr (internal::has_substr<Src>) {
                uri.scheme = src.substr(range.scheme.start, range.scheme.size());
                uri.authority = src.substr(range.authority.start, range.authority.size());
                uri.userinfo = src.substr(range.userinfo.start, range.userinfo.size());
                uri.hostname = src.substr(range.hostname.start, range.hostname.size());
                uri.port = src.substr(range.port.start, range.port.size());
                uri.host = src.substr(range.host.start, range.host.size());
                uri.path = src.substr(range.path.start, range.path.size());
                uri.query = src.substr(range.query.start, range.query.size());
                uri.fragment = src.substr(range.fragment.start, range.fragment.size());
            }
            else if constexpr (internal::is_sequencer<Src>) {
                read_from_seq(src);
            }
            else {
                auto seq = make_ref_seq(src);
                read_from_seq(seq);
            }
        }

        namespace internal {

            template <class T>
            constexpr bool check_relative_uri(Sequencer<T>& seq) {
                auto start = seq.rptr;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c == colon) {
                        seq.rptr = start;
                        return false;  // should be absolute uri
                    }
                    if (c == slash) {
                        seq.rptr = start;
                        return true;  // should be relative uri
                    }
                    seq.consume();
                }
                seq.rptr = start;
                return false;  // should be relative uri?
            }

            template <class T>
            constexpr ParseError read_scheme(URIRange& uri, Sequencer<T>& seq) {
                size_t index = 0;
                uri.scheme.start = seq.rptr;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c == colon) {
                        break;
                    }
                    bool scheme_check = false;
                    if (index == 0) {
                        if (number::is_upper(c) || number::is_lower(c)) {
                            scheme_check = true;
                        }
                    }
                    else {
                        if (number::is_alnum(c) || c == '+' || c == '-' || c == '.') {
                            scheme_check = true;
                        }
                    }
                    if (!scheme_check) {
                        return ParseError::invalid_scheme;
                    }
                    index++;
                    seq.consume();
                }
                if (seq.eos()) {
                    return ParseError::scheme_not_found;
                }
                uri.scheme.end = seq.rptr;  // exclude colon
                seq.consume();
                return ParseError::none;
            }

            template <class T>
            constexpr bool read_authority(URIRange& uri, Sequencer<T>& seq) {
                if (!seq.seek_if("//")) {
                    uri.authority.start = seq.rptr;
                    uri.authority.end = seq.rptr;
                    // no authority
                    return false;
                }
                uri.authority.start = seq.rptr;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c == slash || c == query || c == fragment) {
                        break;
                    }
                    seq.consume();
                }
                uri.authority.end = seq.rptr;
                return true;
            }

            template <class T>
            constexpr ParseError read_path(URIRange& uri, Sequencer<T>& seq, bool has_authority) {
                auto c = seq.current();
                if (has_authority &&
                    (!seq.eos() && c != slash && c != query && c != fragment)) {
                    return ParseError::invalid_path;
                }
                if (c == slash && seq.current(1) == slash) {
                    return ParseError::unexpected_double_slash;
                }
                size_t segment = c == slash ? 1 : 0;
                uri.path.start = seq.rptr;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c == query || c == fragment) {
                        break;
                    }
                    if (segment == 0) {
                        if (c == colon) {
                            return ParseError::unexpected_colon;
                        }
                    }
                    if (c == slash) {
                        segment++;
                    }
                    seq.consume();
                }
                uri.path.end = seq.rptr;
                return ParseError::none;
            }

            template <class T>
            constexpr void read_query(URIRange& uri, Sequencer<T>& seq) {
                if (seq.current() != query) {
                    uri.query.start = uri.query.end = seq.rptr;
                    return;  // no qeury
                }
                uri.query.start = seq.rptr;
                while (!seq.eos()) {
                    auto c = seq.current();
                    if (c == fragment) {
                        break;
                    }
                    seq.consume();
                }
                uri.query.end = seq.rptr;
            }

            template <class T>
            constexpr void read_fragment(URIRange& uri, Sequencer<T>& seq) {
                if (seq.current() != fragment) {
                    uri.fragment.start = uri.fragment.end = seq.rptr;
                    return;  // no qeury
                }
                uri.fragment.start = seq.rptr;
                while (!seq.eos()) {
                    seq.consume();
                }
                uri.fragment.end = seq.rptr;
            }

            template <class T>
            constexpr ParseError read_userinfo(URIRange& uri, Sequencer<T>& seq, Range authority) {
                const auto start = seq.rptr;
                bool has_at = false;
                while (seq.rptr < authority.end) {
                    if (seq.current() == at_sign) {
                        has_at = true;
                        break;
                    }
                    seq.consume();
                }
                seq.rptr = start;
                uri.userinfo.start = seq.rptr;
                if (has_at) {
                    while (seq.current() != at_sign) {
                        seq.consume();
                    }
                    if (seq.current() != at_sign) {
                        return ParseError::no_at_mark_bug;
                    }
                    uri.userinfo.end = seq.rptr;
                    seq.consume();
                }
                else {
                    uri.userinfo.end = uri.userinfo.start;
                }
                return ParseError::none;
            }

            template <class T>
            constexpr ParseError split_authority(URIRange& uri, Sequencer<T>& seq, bool has_user, bool has_port) {
                seq.rptr = uri.authority.start;
                if (has_user) {
                    auto err = read_userinfo(uri, seq, uri.authority);
                    if (err != ParseError::none) {
                        return err;
                    }
                }
                uri.hostname.start = seq.rptr;
                if (seq.consume_if(begin_ipv6)) {
                    bool v6ok = false;
                    size_t index = 0;
                    while (seq.rptr < uri.authority.end) {
                        auto c = seq.current();
                        seq.consume();
                        if (c == end_ipv6) {
                            v6ok = index != 0;
                            break;
                        }
                        index++;
                    }
                    if (!v6ok) {
                        return ParseError::invalid_ipv6_hostname;
                    }
                }
                else {
                    const size_t ptr = seq.rptr;
                    size_t port_index = ptr;
                    if (has_port) {
                        has_port = false;
                        while (seq.rptr < uri.authority.end) {
                            if (seq.current() == colon) {
                                port_index = seq.rptr;
                                has_port = true;
                            }
                            seq.consume();
                        }
                        seq.rptr = ptr;
                    }
                    while (seq.rptr < uri.authority.end) {
                        if (has_port) {
                            if (seq.rptr == port_index) {
                                break;
                            }
                        }
                        seq.consume();
                    }
                }
                uri.hostname.end = seq.rptr;
                if (has_port) {
                    if (seq.consume_if(colon)) {
                        uri.port.start = seq.rptr;
                        while (seq.rptr < uri.authority.end) {
                            seq.consume();
                        }
                        uri.port.end = seq.rptr;
                    }
                    else {
                        uri.port.start = uri.port.end = uri.hostname.end;
                    }
                }
                else {
                    uri.port.start = uri.port.end = uri.hostname.end;
                }
                uri.host.start = uri.hostname.start;
                uri.host.end = uri.port.end;
                return ParseError::none;
            }
        }  // namespace internal

        template <class T>
        constexpr ParseError parse_range(URIRange& uri, Sequencer<T>& seq) {
            const auto is_relative = internal::check_relative_uri(seq);
            ParseError err{};
            auto has_err = [&] {
                return err != ParseError::none;
            };
            if (!is_relative) {
                err = internal::read_scheme(uri, seq);
                if (has_err()) {
                    return err;
                }
            }
            auto has_autority = internal::read_authority(uri, seq);
            err = internal::read_path(uri, seq, has_autority);
            if (has_err()) {
                return err;
            }
            internal::read_query(uri, seq);
            internal::read_fragment(uri, seq);
            return ParseError::none;
        }

        template <class String>
        constexpr ParseError parse_range(URIRange& uri, String&& str) {
            auto seq = make_ref_seq(str);
            return parse(uri, seq);
        }

        template <class T>
        constexpr ParseError parse_range_ex(URIRange& uri, Sequencer<T>& seq, bool has_user = true, bool has_port = true) {
            auto err = parse_range(uri, seq);
            if (err != ParseError::none) {
                return err;
            }
            err = internal::split_authority(uri, seq, has_user, has_port);
            if (err != ParseError::none) {
                return err;
            }
            return err;
        }

        template <class String>
        constexpr ParseError parse_range_ex(URIRange& uri, String&& str, bool has_user = true, bool has_port = true) {
            auto seq = make_ref_seq(str);
            return parse_range_ex(uri, seq, has_user, has_port);
        }

        template <class URI, class T>
        constexpr ParseError parse(URI& uri, Sequencer<T>& seq) {
            URIRange range{};
            auto err = parse_range(range, seq);
            if (err != ParseError::none) {
                return err;
            }
            range_to_uri(uri, range, seq);
            return ParseError::none;
        }

        template <class URI, class String>
        constexpr ParseError parse_ex(URI& uri, String&& str, bool has_user = true, bool has_port = true) {
            URIRange range{};
            auto err = parse_range_ex(range, std::forward<String>(str), has_user, has_port);
            if (err != ParseError::none) {
                return err;
            }
            range_to_uri(uri, range, str);
            return ParseError::none;
        }

        namespace test {
            constexpr auto test_range() {
                auto test = [](const char* str, URIRange expected) {
                    URIRange uri{};
                    auto seq = make_ref_seq(str);
                    auto err = parse_range_ex(uri, seq);
                    auto report_error = [&](auto at) {
                        [](auto... args) {
                            throw "error";
                        }(err, err_msg(err), uri, expected, at);
                    };
                    if (err != ParseError::none) {
                        report_error("parse_range_ex");
                    }
                    if (uri.scheme.start != expected.scheme.start || uri.scheme.end != expected.scheme.end) {
                        report_error("scheme");
                    }
                    if (uri.authority.start != expected.authority.start || uri.authority.end != expected.authority.end) {
                        report_error("authority");
                    }
                    if (uri.userinfo.start != expected.userinfo.start || uri.userinfo.end != expected.userinfo.end) {
                        report_error("userinfo");
                    }
                    if (uri.hostname.start != expected.hostname.start || uri.hostname.end != expected.hostname.end) {
                        report_error("hostname");
                    }
                    if (uri.port.start != expected.port.start || uri.port.end != expected.port.end) {
                        report_error("port");
                    }
                    if (uri.host.start != expected.host.start || uri.host.end != expected.host.end) {
                        report_error("host");
                    }
                    if (uri.path.start != expected.path.start || uri.path.end != expected.path.end) {
                        report_error("path");
                    }
                    if (uri.query.start != expected.query.start || uri.query.end != expected.query.end) {
                        report_error("query");
                    }
                    if (uri.fragment.start != expected.fragment.start || uri.fragment.end != expected.fragment.end) {
                        report_error("fragment");
                    }
                    return true;
                };

                constexpr auto str = "http://www.example.com:8080/path/to/file?query#fragment";
                // scheme: http
                // authority: www.example.com:8080
                // userinfo: ""
                // hostname: www.example.com
                // port: 8080
                // host: www.example.com:8080
                // path: /path/to/file
                // query: ?query
                // fragment: #fragment
                URIRange expected1{
                    {0, 4},
                    {7, 27},
                    {7, 7},
                    {7, 22},
                    {23, 27},
                    {7, 27},
                    {27, 40},
                    {40, 46},
                    {46, 55},
                };
                if (!test(str, expected1)) {
                    return false;
                }

                constexpr auto str2 = "/relative/path/to/file?query#fragment";
                // scheme: ""
                // authority: ""
                // userinfo: ""
                // hostname: ""
                // port: ""
                // path: /relative/path/to/file
                // query: ?query
                // fragment: #fragment
                URIRange expected2{
                    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 22}, {22, 28}, {28, 37}};
                if (!test(str2, expected2)) {
                    return false;
                }

                constexpr auto mailto = "mailto:hello@example.com";
                // scheme: mailto
                // authority: ""
                // userinfo: ""
                // hostname: ""
                // port: ""
                // path: hello@example.com
                // query: ""
                // fragment: ""
                URIRange expected3{
                    {0, 6}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 24}, {24, 24}, {24, 24}};
                if (!test(mailto, expected3)) {
                    return false;
                }

                return true;
            }

            static_assert(test_range());
        }  // namespace test

        template <class Char>
        struct URIFixed {
            const Char* raw_uri;
            view::basic_rvec<Char> scheme;
            view::basic_rvec<Char> authority;
            view::basic_rvec<Char> userinfo;
            view::basic_rvec<Char> hostname;
            view::basic_rvec<Char> port;
            view::basic_rvec<Char> host;
            view::basic_rvec<Char> path;
            view::basic_rvec<Char> query;
            view::basic_rvec<Char> fragment;
        };

        template <class Char>
        constexpr auto fixed(const Char* eval, bool has_user = true, bool has_port = true) {
            URIRange range{};
            auto err = parse_range_ex(range, eval, has_user, has_port);
            if (err != ParseError::none) {
                return URIFixed<Char>{};
            }
            URIFixed<Char> uri{};
            uri.raw_uri = eval;
            range_to_uri(uri, range, view::basic_rvec<Char>(eval, futils::strlen(eval)));
            return uri;
        }

        template <class URI>
        constexpr void tidy(URI& uri, bool pass_mode = true) {
            if (pass_mode) {
                if (strutil::ends_with(uri.scheme, ":")) {
                    uri.scheme.pop_back();
                }
                if (strutil::starts_with(uri.port, ":")) {
                    uri.scheme.erase(0, 1);
                }
            }
            else {
                if (!strutil::equal(uri.scheme, "") && !strutil::ends_with(uri.scheme, ":")) {
                    uri.scheme.push_back(colon);
                }
                if (!strutil::equal(uri.port, "") && !strutil::starts_with(uri.port, ":")) {
                    uri.port.insert(uri.port.begin(), colon);
                }
            }
        }

        template <class String>
        struct URI {
            String scheme;
            String authority;
            String userinfo;
            String hostname;
            String port;
            String host;
            String path;
            String query;
            String fragment;
        };

        template <class Out, class String>
        void composition_uri(Out& str, const URI<String>& uri, bool use_splited = true) {
            if (uri.scheme.size()) {
                strutil::append(str, uri.scheme);
                if (uri.scheme[uri.scheme.size() - 1] != ':') {
                    str.push_back(':');
                }
            }
            if (use_splited) {
                if (uri.hostname.size()) {
                    strutil::append(str, "//");
                    if (uri.userinfo.size()) {
                        strutil::append(str, uri.userinfo);
                        if (uri.userinfo[uri.userinfo.size() - 1] != '@') {
                            str.push_back('@');
                        }
                    }
                    strutil::append(str, uri.hostname);
                    if (uri.port.size()) {
                        if (uri.port[0] != ':') {
                            str.push_back(':');
                        }
                        strutil::append(str, uri.port);
                    }
                }
            }
            else {
                if (uri.authority.size()) {
                    strutil::appends(str, "//", uri.authority);
                }
            }
            if (uri.path.size()) {
                strutil::append(str, uri.path);
            }
            if (uri.query.size()) {
                if (uri.query[0] != '?') {
                    str.push_back('?');
                }
                strutil::append(str, uri.query);
            }
            if (uri.fragment.size()) {
                if (uri.query[0] != '#') {
                    str.push_back('#');
                }
                strutil::append(str, uri.query);
            }
        }

        template <class Out, class String>
        Out composition_uri(const URI<String>& uri, bool use_splited = true) {
            Out out;
            composition_uri(out, uri, use_splited);
            return out;
        }

        template <class String>
        String composition_uri(const URI<String>& uri, bool use_splited = true) {
            return composition_uri<String, String>(uri, use_splited);
        }

    }  // namespace uri
}  // namespace futils
