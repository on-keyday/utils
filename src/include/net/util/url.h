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
        struct URL {
            wrap::string scheme;
            wrap::string user;
            wrap::string password;
            wrap::string host;
            wrap::string port;
            wrap::string path;
            wrap::string query;
            wrap::string tag;
            wrap::string other;
        };

        namespace internal {
            template <class T>
            void parse_user(bool& unknown_data, Sequencer<T>& seq, URL& parsed) {
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
                            parsed.user.password.push_bacck(seq.current());
                        }
                    }
                    seq.consume();
                }
                if (at_first) {
                    unknown_data = true;
                }
            }

            template <class T>
            void parse_host(bool& unknown_data, Sequencer<T>& seq, URL& parsed) {
                bool at_first = false;
                bool on_port = false;
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
                }
                if (at_first) {
                    unknown_data = true;
                }
            }
        }  // namespace internal

        template <class String>
        bool parse_url(String&& str, URL& parsed) {
            auto seq = make_ref_seq(str);
            wrap::string current;
            bool has_user = false;
            bool has_scheme = false;
            bool unknown_data = true;
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
            seq.seek_if("//");
            if (has_user) {
                internal::parse_user(unknown_data, seq, parsed);
            }
            if (!unknown_data) {
                internal::parse_host(unknown_data, seq, parsed);
            }
        }

    }  // namespace net
}  // namespace utils
