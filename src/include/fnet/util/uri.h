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

        template <class URI, class T>
        constexpr ParseError read_scheme(URI& uri, Sequencer<T>& seq) {
            size_t index = 0;
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
                uri.scheme.push_back(c);
                seq.consume();
            }
            if (seq.eos()) {
                return ParseError::scheme_not_found;
            }
            uri.scheme.push_back(colon);
            seq.consume();
            return ParseError::none;
        }

        template <class URI, class T>
        constexpr bool read_authority(URI& uri, Sequencer<T>& seq) {
            if (!seq.seek_if("//")) {
                // no authority
                return false;
            }
            while (!seq.eos()) {
                auto c = seq.current();
                if (c == slash || c == query || c == fragment) {
                    break;
                }
                uri.authority.push_back(c);
                seq.consume();
            }
            return true;
        }

        template <class URI, class T>
        constexpr ParseError read_path(URI& uri, Sequencer<T>& seq, bool has_authority) {
            auto c = seq.current();
            if (has_authority &&
                (!seq.eos() && c != slash && c != query && c != fragment)) {
                return ParseError::invalid_path;
            }
            if (c == slash && seq.current(1) == slash) {
                return ParseError::unexpected_double_slash;
            }
            size_t segment = c == slash ? 1 : 0;
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
                uri.path.push_back(c);
                seq.consume();
            }
            return ParseError::none;
        }

        template <class URI, class T>
        constexpr void read_query(URI& uri, Sequencer<T>& seq) {
            if (seq.current() != query) {
                return;  // no qeury
            }
            while (!seq.eos()) {
                auto c = seq.current();
                if (c == fragment) {
                    break;
                }
                uri.query.push_back(c);
                seq.consume();
            }
        }

        template <class URI, class T>
        constexpr void read_fragment(URI& uri, Sequencer<T>& seq) {
            if (seq.current() != fragment) {
                return;  // no qeury
            }
            while (!seq.eos()) {
                auto c = seq.current();
                uri.fragment.push_back(c);
                seq.consume();
            }
        }

        template <class URI, class T>
        constexpr ParseError parse(URI& uri, Sequencer<T>& seq) {
            const auto is_relative = check_relative_uri(seq);
            ParseError err{};
            auto has_err = [&] {
                return err != ParseError::none;
            };
            if (!is_relative) {
                err = read_scheme(uri, seq);
                if (has_err()) {
                    return err;
                }
            }
            auto has_autority = read_authority(uri, seq);
            err = read_path(uri, seq, has_autority);
            if (has_err()) {
                return err;
            }
            read_query(uri, seq);
            read_fragment(uri, seq);
            return ParseError::none;
        }

        template <class URI, class String>
        constexpr ParseError parse(URI& uri, String&& str) {
            auto seq = make_ref_seq(str);
            return parse(uri, seq);
        }

        template <class URI, class T>
        constexpr ParseError read_userinfo(URI& uri, Sequencer<T>& seq) {
            const auto start = seq.rptr;
            bool has_at = false;
            while (!seq.eos()) {
                if (seq.current() == at_sign) {
                    has_at = true;
                    break;
                }
                seq.consume();
            }
            seq.rptr = start;
            if (has_at) {
                while (seq.current() != at_sign) {
                    uri.userinfo.push_back(seq.current());
                    seq.consume();
                }
                if (seq.current() != at_sign) {
                    int* ptr = nullptr;
                    *ptr = 0;  // terminate
                }
                uri.userinfo.push_back(seq.current());
                seq.consume();
            }
            return ParseError::none;
        }

        template <class URI, class T>
        constexpr ParseError split_authority(URI& uri, Sequencer<T>& seq, bool has_user, bool has_port) {
            if (has_user) {
                auto err = read_userinfo(uri, seq);
                if (err != ParseError::none) {
                    return err;
                }
            }
            if (seq.consume_if(begin_ipv6)) {
                uri.hostname.push_back(begin_ipv6);
                bool v6ok = false;
                size_t index = 0;
                while (!seq.eos()) {
                    auto c = seq.current();
                    uri.hostname.push_back(c);
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
                    while (!seq.eos()) {
                        if (seq.current() == colon) {
                            port_index = seq.rptr;
                            has_port = true;
                        }
                        seq.consume();
                    }
                    seq.rptr = ptr;
                }
                while (!seq.eos()) {
                    if (has_port) {
                        if (seq.rptr == port_index) {
                            break;
                        }
                    }
                    uri.hostname.push_back(seq.current());
                    seq.consume();
                }
            }
            if (has_port) {
                if (seq.consume_if(colon)) {
                    uri.port.push_back(colon);
                    while (!seq.eos()) {
                        uri.port.push_back(seq.current());
                        seq.consume();
                    }
                }
            }
            return ParseError::none;
        }

        template <class URI, class T>
        constexpr ParseError parse_ex(URI& uri, Sequencer<T>& seq, bool has_user = true, bool has_port = true) {
            auto err = parse(uri, seq);
            if (err != ParseError::none) {
                return err;
            }
            auto ex = make_ref_seq(uri.authority);
            err = split_authority(uri, ex, has_user, has_port);
            if (err != ParseError::none) {
                return err;
            }
            return err;
        }

        template <class URI, class String>
        constexpr ParseError parse_ex(URI& uri, String&& str, bool has_user = true, bool has_port = true) {
            auto seq = make_ref_seq(str);
            return parse_ex(uri, seq, has_user, has_port);
        }

        struct URILength {
            using Counter = helper::CountPushBacker<>;
            Counter scheme;
            Counter authority;
            Counter userinfo;
            Counter hostname;
            Counter port;
            Counter path;
            Counter query;
            Counter fragment;
        };

        template <class Char, size_t len_max>
        struct URIFixed {
            number::Array<Char, len_max, true> scheme;
            number::Array<Char, len_max, true> authority;
            number::Array<Char, len_max, true> userinfo;
            number::Array<Char, len_max, true> hostname;
            number::Array<Char, len_max, true> port;
            number::Array<Char, len_max, true> path;
            number::Array<Char, len_max, true> query;
            number::Array<Char, len_max, true> fragment;
        };

        template <size_t len, class Char>
        consteval auto fixed(const Char (&eval)[len], bool has_user = true, bool has_port = true) {
            using URI = URIFixed<Char, len>;
            URI uri{};
            auto err = parse_ex(uri, eval, has_user, has_port);
            if (err != ParseError::none) {
                return URI{};
            }
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
