/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// url - parse url data

#pragma once

#include "../../core/sequencer.h"
#include "../../helper/strutil.h"
#include "../../wrap/lite/string.h"

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
            bool has_dobule_slash = false;
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
                if (seq.seek_if("[")) {
                    helper::read_until(parsed.host, seq, "]");
                }
                while (!seq.eos()) {
                    if (seq.current() == '/') {
                        if (at_first) {
                            unknown_data = true;
                        }
                        break;
                    }
                    else if (seq.current() == ':') {
                        on_port = true;
                        at_first = true;
                    }
                    else {
                        if (on_port) {
                            parsed.port.push_back(seq.current());
                        }
                        else {
                            parsed.host.push_back(seq.current());
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
        }  // namespace internal

        template <class String>
        void rough_uri_parse(String&& str, URI& parsed) {
            auto seq = make_ref_seq(str);
            wrap::string current;
            bool has_user = false;
            bool has_scheme = false;
            bool unknown_data = false;
            bool no_host = false;
            if (helper::starts_with(str, "/") && !helper::starts_with("//")) {
                no_host = true;
            }
            if (helper::contains(str, "@")) {
                has_user = true;
            }
            while (!seq.eos()) {
                if (seq.match("//")) {
                    break;
                }
                if (seq.match("/")) {
                    unknown_data = true;
                    break;
                }
                if (seq.match(".")) {
                    break;
                }
                if (seq.match(":")) {
                    has_scheme = true;
                    break;
                }
                seq.consume();
            }
            seq.seek(0);
            if (has_scheme) {
                helper::read_until(parsed.scheme, seq, ":");
            }
            if (!no_host) {
                parsed.has_dobule_slash = seq.seek_if("//");
                if (!parsed.has_dobule_slash) {
                    no_host = true;
                    has_user = false;
                    unknown_data = true;
                }
            }
            if (has_user) {
                internal::parse_user(unknown_data, seq, parsed);
            }
            if (!no_host && !unknown_data) {
                internal::parse_host(unknown_data, seq, parsed);
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
    }  // namespace net
}  // namespace utils
