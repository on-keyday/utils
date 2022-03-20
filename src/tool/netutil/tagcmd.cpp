/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net/http/value.h"
using namespace utils;
namespace netutil {
    bool to_json(TagFlag flag, auto& json) {
        number::Array<50, char, true> arr;
        bool first = true;
        auto write = [&](auto&&... v) {
            if (!first) {
            }
            first = true;
        };
        if (any(flag & TagFlag::redirect)) {
        }
        json = arr.c_str();
        return true;
    }

    bool to_json(const TagCommand& cmd, auto& json) {
        JSON_PARAM_BEGIN(cmd, json)
        TO_JSON_PARAM(file, "file")
        TO_JSON_PARAM(flag, "flag")
        JSON_PARAM_END()
    }

    bool parse_tagcommand(const wrap::string& str, TagCommand& cmd, wrap::string& err) {
        if (!helper::sandwiched(str, "(", ")")) {
            err = "not a tag command";
            return false;
        }
        auto seq = make_ref_seq(str);
        seq.consume();
        auto read = [&](auto& out) {
            out.clear();
            return helper::read_whilef<true>(out, seq, [](auto c) {
                return c != ')' && c != ',';
            });
        };
        auto set_flag = [&](TagFlag flag) {
            if (seq.seek_if("=false")) {
                cmd.flag &= ~flag;
            }
            else if (seq.seek_if("=true") || true) {
                cmd.flag |= flag;
            }
        };
        while (true) {
            if (seq.seek_if("file=")) {
                if (!read(cmd.file)) {
                    err = "failed to read file name";
                    return false;
                }
            }
            else if (seq.seek_if("method=")) {
                if (!read(cmd.method)) {
                    err = "failed to read http method";
                }
                if (cmd.method != "CONNECT") {
                    for (auto& v : net::h1value::methods) {
                        if (cmd.method == v) {
                            goto END;
                        }
                    }
                }
                err = "http method " + cmd.method + " is not allowed";
            }
            else if (seq.seek_if("unsafe_method=")) {
                if (!read(cmd.method)) {
                    err = "failed to read http method";
                }
            }
            else if (seq.seek_if("redirect")) {
                set_flag(TagFlag::redirect);
            }
            else if (seq.seek_if("save_if_succeed")) {
                set_flag(TagFlag::save_if_succeed);
            }
            else if (seq.seek_if("show_request")) {
                set_flag(TagFlag::show_request);
            }
            else if (seq.seek_if("show_response")) {
                set_flag(TagFlag::show_response);
            }
            else if (seq.seek_if("show_body")) {
                set_flag(TagFlag::show_body);
            }
        END:
            if (seq.consume_if(',')) {
                continue;
            }
            if (!seq.consume_if(')')) {
                err = "expect ) but not";
                return false;
            }
            break;
        }
    }
}  // namespace netutil