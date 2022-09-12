/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/net/http/predefined.h"
#include "../../include/helper/cond_writer.h"

using namespace utils;
namespace netutil {
    bool to_json(TagFlag flag, auto& json) {
        number::Array<50, char, true> arr;
        auto write = helper::first_late_writer(arr, " | ");
        if (any(flag & TagFlag::redirect)) {
            write("redirect");
        }
        if (any(flag & TagFlag::save_if_succeed)) {
            write("save_if");
        }
        if (any(flag & TagFlag::show_request)) {
            write("show_request");
        }
        if (any(flag & TagFlag::show_response)) {
            write("show_response");
        }
        if (flag == TagFlag::none) {
            write("none");
        }
        json = arr.c_str();
        return true;
    }

    bool to_json(const TagCommand& cmd, auto& json) {
        JSON_PARAM_BEGIN(cmd, json)
        TO_JSON_PARAM(file, "file")
        TO_JSON_PARAM(flag, "flag")
        TO_JSON_PARAM(ipver, "ipver")
        TO_JSON_PARAM(data_loc, "data_loc")
        TO_JSON_PARAM(method, "method")
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
            else if (seq.seek_if("data=")) {
                if (!read(cmd.data_loc)) {
                    err = "failed to read payload file name";
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
            else if (seq.seek_if("ipver=")) {
                if (seq.seek_if("4")) {
                    cmd.ipver = 4;
                }
                else if (seq.seek_if("6")) {
                    cmd.ipver = 6;
                }
                else if (seq.seek_if("0")) {
                    cmd.ipver = 0;
                }
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
        return true;
    }
}  // namespace netutil