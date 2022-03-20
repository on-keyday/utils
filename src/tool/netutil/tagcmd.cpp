/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
using namespace utils;
namespace netutil {
    bool parse_tagcommand(const wrap::string& str, TagCommand& cmd) {
        if (!helper::sandwiched(str, "(", ")")) {
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
        while (true) {
            if (seq.seek_if("file=")) {
                if (!read(cmd.file)) {
                }
            }
            else if (seq.seek_if("method=")) {
            }
            if (seq.consume_if(',')) {
                continue;
            }
            if (!seq.consume_if(')')) {
                return false;
            }
        }
    }
}  // namespace netutil