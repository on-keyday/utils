/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// uri - parse uri
#pragma once

#include "../core/sequencer.h"
#include "../helper/strutil.h"
#include "../wrap/light/string.h"
#include "../helper/readutil.h"
#include "../number/array.h"

namespace utils {
    namespace net {
        struct URI {
            wrap::string scheme;
            wrap::string user;
            wrap::string password;
            wrap::string host;
            wrap::string port;
            wrap::string path;
            wrap::string query;
            wrap::string tag;
            wrap::string other;
            bool has_double_slash = false;

            wrap::string path_query() {
                wrap::string ret;
                ret += path;
                if (query.size() && query.front() != '?') {
                    ret += '?';
                }
                ret += query;
                return ret;
            }

            wrap::string host_port() {
                wrap::string ret;
                ret += host;
                if (port.size() && port.front() != ':') {
                    port += ':';
                }
                ret += port;
                return ret;
            }

            wrap::string tags() {
                wrap::string ret;
                if (tag.size() && tag.front() != '#') {
                    ret += '#';
                }
                ret += tag;
                return ret;
            }

            wrap::string to_string() {
                wrap::string ret;
                ret += scheme;
                if (scheme.size() && scheme.back() != ':') {
                    ret += ':';
                }
                if (has_double_slash) {
                    ret += "//";
                }
                if (user.size()) {
                    ret += user;
                    if (password.size()) {
                        ret += ':';
                        ret += password;
                    }
                    ret += '@';
                }
                ret += host_port();
                ret += path_query();
                ret += tags();
                ret += other;
                return ret;
            }
        };

        namespace internal {
            template <class T>
            void parse_user(bool& unknown_data, Sequencer<T>& seq, URI& parsed) {
                bool at_first = true;
                bool on_password = false;
                while (!seq.eos()) {
                    if (seq.current() == '@') {
                        if (at_first) {
                            unknown_data = true;
                            break;
                        }
                        seq.consume();
                        break;
                    }
                    else if (seq.current() == ':') {
                        if (at_first) {
                            unknown_data = true;
                            break;
                        }
                        on_password = true;
                        at_first = true;
                    }
                    else {
                        if (on_password) {
                            parsed.password.push_back(seq.current());
                        }
                        else {
                            parsed.user.push_back(seq.current());
                        }
                    }
                    seq.consume();
                    at_first = false;
                }
                if (at_first) {
                    unknown_data = true;
                }
            }

            template <class T>
            void parse_host(bool& unknown_data, Sequencer<T>& seq, URI& parsed) {
                bool at_first = false;
                bool on_port = false;
                bool has_dot = false;
                if (seq.seek_if("[")) {
                    parsed.host.push_back('[');
                    if (helper::read_until(parsed.host, seq, "]")) {
                        parsed.host.push_back(']');
                    }
                    has_dot = true;
                }
                while (!seq.eos()) {
                    if (seq.current() == '/' || seq.current() == '?' || seq.current() == '#') {
                        break;
                    }
                    else if (seq.current() == ':') {
                        on_port = true;
                        at_first = true;
                        parsed.port.push_back(':');
                    }
                    else {
                        if (on_port) {
                            parsed.port.push_back(seq.current());
                        }
                        else {
                            if (seq.current() == '.') {
                                has_dot = true;
                            }
                            parsed.host.push_back(seq.current());
                        }
                    }
                    seq.consume();
                    at_first = false;
                }
                if (!has_dot) {
                    unknown_data = true;
                    parsed.other = std::move(parsed.host);
                    if (on_port) {
                        parsed.other.append(parsed.port);
                        parsed.port.clear();
                    }
                }
                if (at_first) {
                    unknown_data = true;
                }
            }

            template <class T>
            void parse_path(bool& unknown_data, Sequencer<T>& seq, URI& parsed) {
                bool on_query = false;
                bool on_tag = false;
                while (!seq.eos()) {
                    if (seq.current() == '?') {
                        on_query = true;
                    }
                    else if (seq.current() == '#') {
                        on_tag = true;
                    }
                    if (on_tag) {
                        parsed.tag.push_back(seq.current());
                    }
                    else if (on_query) {
                        parsed.query.push_back(seq.current());
                    }
                    else {
                        parsed.path.push_back(seq.current());
                    }
                    seq.consume();
                }
            }

            enum class URIParts : std::uint8_t {
                opaque,
                shceme,
                double_slash,
                user,
                user_with_password,
                password,
                host,
                ipv6_host,
                port,
                abstract_path,
                relative_path,
                query,
                tag,
            };

            struct URIStruct {
                using Struct = number::Array<10, URIParts>;
                Struct suggest;
            };

            template <class T>
            URIStruct guess_uri_structure(T&& seq) {
                constexpr size_t npos = ~0;
                constexpr size_t first = 0;
                URIStruct uri{};
                // get element position
                auto at_ = helper::find(seq, "@");
                size_t colons[4] = {0};
                colons[0] = helper::find(seq, ":");
                colons[1] = helper::find(seq, ":", 0, colons[0] + 1);
                colons[2] = helper::find(seq, ":", 0, colons[1] + 1);
                auto ss_begin = helper::find(seq, "[");
                auto ss_end = helper::find(seq, "]");
                auto double_slash = helper::find(seq, "//");
                auto query = helper::find(seq, "?");
                auto tag = helper::find(seq, "#");
                size_t ss_colon = npos;
                if (ss_end != npos) {
                    auto tmp = make_ref_seq(seq);
                    tmp.rptr = ss_end + 1;
                    if (tmp.current() == ':') {
                        // [ipv6_address]:port
                        ss_colon = ss_end + 1;
                    }
                }
                size_t slash = 0;
                if (double_slash != npos) {
                    slash = helper::find(seq, "/", 2);
                }
                else {
                    slash = helper::find(seq, "/");
                }
                // make pattern
                size_t colon_index = 0;
                if (slash < at_) {
                    // /using@path
                    at_ = npos;
                }
                if (double_slash == first) {
                    /* //host... */
                    uri.suggest.push_back(URIParts::double_slash);
                }
                else if (double_slash != npos && colons[colon_index] == double_slash - 1) {
                    // scheme://host...
                    uri.suggest.push_back(URIParts::shceme);
                    uri.suggest.push_back(URIParts::double_slash);
                    colon_index++;
                }
                if (at_ != npos) {
                    if (colons[colon_index] < at_) {
                        // user:password@host...
                        uri.suggest.push_back(URIParts::user_with_password);
                        uri.suggest.push_back(URIParts::password);
                        colon_index++;
                    }
                    else {
                        // user@host...
                        uri.suggest.push_back(URIParts::user);
                    }
                }
                if (double_slash != npos &&
                    double_slash + 2 == slash) {
                    /* ...///path */
                    // skip
                }
                else if (slash == first || query == first || tag == first) {
                    // /path
                    // ?query
                    // #tag
                    // skip
                }
                else if (at_ != npos &&
                         at_ + 1 == slash) {
                    // ...@/path
                    // skip
                    // TODO: is this Ok?
                }
                else if (ss_begin != npos && ss_end != npos &&
                         ss_begin < ss_end && ss_end < slash) {
                    // ...[ipv6str]/path
                    uri.suggest.push_back(URIParts::ipv6_host);
                    if (ss_colon != npos) {
                        uri.suggest.push_back(URIParts::port);
                    }
                }
                else if (colons[colon_index] < slash) {
                    // ...host:port/path
                    uri.suggest.push_back(URIParts::host);
                    uri.suggest.push_back(URIParts::port);
                }
                else {
                    uri.suggest.push_back(URIParts::host);
                }
                if (slash != npos) {
                    uri.suggest.push_back(URIParts::abstract_path);
                }
                if (query != npos && (slash == npos || slash < query)) {
                    uri.suggest.push_back(URIParts::query);
                }
                if (tag != npos &&
                    (slash == npos || slash < tag) &&
                    (query == npos || slash < tag)) {
                    uri.suggest.push_back(URIParts::tag);
                }
                return uri;
            }
        }  // namespace internal

        template <class String>
        void rough_uri_parse(String&& str, URI& parsed) {
            enum {
                unknown,
                abstract_path,
                user_name,
                ipv6_addr,
                without_scheme,
                relative_path,
                host_name,
                with_scheme,
            } expect = unknown;
            auto seq = make_ref_seq(str);
            wrap::string current;
            bool has_user = false;
            bool has_scheme = false;
            bool unknown_data = false;
            bool no_host = false;
            if (helper::starts_with(str, "/") && !helper::starts_with(str, "//")) {
                // abstract path
                no_host = true;
                expect = abstract_path;
            }

            if (!no_host && helper::contains(str, "@")) {
                // user name
                has_user = true;
            }
            while (!seq.eos()) {
                if (seq.match("[")) {
                    // ipv6 address
                    expect = ipv6_addr;
                    break;
                }
                if (helper::starts_with(str, "//")) {
                    // without scheme
                    expect = without_scheme;
                    break;
                }
                if (seq.match("/")) {
                    // relative path
                    expect = relative_path;
                    break;
                }
                if (seq.match(".")) {
                    // host name
                    expect = host_name;
                    break;
                }
                if (seq.match(":")) {
                    // with scheme
                    has_scheme = true;
                    expect = with_scheme;
                    break;
                }
                seq.consume();
            }
            seq.seek(0);
            if (has_scheme) {
                helper::read_until(parsed.scheme, seq, ":");
                parsed.scheme.push_back(':');
                expect = host_name;
            }
            if (!no_host) {
                parsed.has_double_slash = seq.seek_if("//");
                if (!parsed.has_double_slash && has_scheme && !has_user) {
                    no_host = true;
                    unknown_data = true;
                }
            }
            if (has_user) {
                internal::parse_user(unknown_data, seq, parsed);
            }
            if (!no_host && !unknown_data) {
                internal::parse_host(unknown_data, seq, parsed);
                if (unknown_data) {
                    if (seq.current() == '/' || seq.eos()) {
                        parsed.path = std::move(parsed.other);
                        unknown_data = false;
                    }
                }
            }
            if (!unknown_data) {
                internal::parse_path(unknown_data, seq, parsed);
            }
            if (unknown_data) {
                while (!seq.eos()) {
                    parsed.other.push_back(seq.current());
                    seq.consume();
                }
            }
        }

        inline void uri_tidy(net::URI& uri) {
            if (uri.scheme.size() && uri.scheme.back() == ':') {
                uri.scheme.pop_back();
            }
            if (uri.host.size()) {
                if (uri.host.front() == '[' && uri.host.back() == ']') {
                    uri.host.pop_back();
                    uri.host.erase(0, 1);
                }
            }
            if (uri.port.size() && uri.port.front() == ':') {
                uri.port.erase(0, 1);
            }
            if (uri.query.size() && uri.query.front() == '?') {
                uri.query.erase(0, 1);
            }
            if (uri.tag.size() && uri.tag.front() == '#') {
                uri.tag.erase(0, 1);
            }
        }
    }  // namespace net
}  // namespace utils
