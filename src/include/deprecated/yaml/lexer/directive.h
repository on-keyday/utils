/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// directive
#pragma once
#include "lexer.h"
#include "basic.h"

namespace utils::yaml::lexer {

    struct DirectiveLexer : LexerCommon {
        using LexerCommon::LexerCommon;

        view::rvec read_directive() {
            if (auto v = expect("%"); v.null()) {
                return {};
            }
        }

        view::rvec read_YAML_version() {
            const auto begin = r.offset();
            auto bs = BasicLexer{r};
            if (bs.read_digits().null()) {
                return {};
            }
            if (expect(".").null()) {
                return {};
            }
            if (bs.read_digits().null()) {
                return {};
            }
            return r.read().substr(begin);
        }

        view::rvec read_YAML_directive() {
            const auto begin = r.offset();
            if (auto v = expect("YAML"); v.null()) {
                return {};
            }
            auto bs = BasicLexer{r};
            if (bs.read_separate_in_line().null()) {
                return {};
            }
            if (read_YAML_version().null()) {
                return {};
            }
            return r.read().substr(begin);
        }

        view::rvec read_TAG_handle() {
            const auto begin = r.offset();
            if (auto v = expect("!"); !v.null()) {
                return v;
            }
            if (auto v = expect("!"); !v.null()) {
                return v;
            }
        }

        view::rvec read_TAG_directive() {
            const auto begin = r.offset();
            if (auto v = expect("TAG"); v.null()) {
                return {};
            }
            auto bs = BasicLexer{r};
            if (bs.read_separate_in_line().null()) {
                return {};
            }
        }

        view::rvec read_escaped() {
            auto seq = remain();
            if (!seq.consume_if('\\')) {
                return {};
            }
            byte single[] = "0abt\tnvfre \"/\\N_LP";
            for (auto c : single) {
                if (!c) {
                    break;
                }
                if (seq.consume_if(c)) {
                    return read(seq.rptr);
                }
            }
            if (seq.consume_if('x')) {
                return read(4);
            }
            if (seq.consume_if('u')) {
                return read(6);
            }
            if (seq.consume_if('U')) {
                return read(10);
            }
            return {};
        }

        view::rvec read_double_quoted(bool allow_line) {
            auto begin = r.offset();
            if (auto v = expect("\""); v.null()) {
                return {};
            }
            while (true) {
                if (auto esc = read_escaped(); !esc.null()) {
                    continue;
                }
                auto n = read(1);
                if (n.null() || n[0] == '\\') {
                    r.reset(begin);
                    return {};
                }
                if (!allow_line && (n[0] == '\r' || n[0] == '\n')) {
                    r.reset(begin);
                    return {};
                }
                if (n[0] == '\"') {
                    break;
                }
            }
            return r.read().substr(begin);
        }
    };

    enum class DirectiveType {
        YAML,
        TAG,
        reserved,
    };

    struct Directive {
        DirectiveType type = DirectiveType::reserved;
        view::rvec data;
    };

}  // namespace utils::yaml::lexer
