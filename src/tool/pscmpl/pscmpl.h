/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <parser/defs/command_expr.h>
#include <parser/defs/jsoncvt.h>
#include <wrap/light/string.h>
#include <wrap/light/vector.h>
#include <wrap/light/hash_map.h>
using namespace utils::parser;

namespace pscmpl {

    template <class T>
    auto define_parser(const utils::Sequencer<T>& seq, expr::PlaceHolder*& ph) {
        using namespace utils::wrap;
        auto rp = expr::define_replacement(ph);
        auto mul = expr::define_binary(
            rp,
            expr::Ops{"*", expr::Op::mul},
            expr::Ops{"/", expr::Op::div},
            expr::Ops{"%", expr::Op::mod});
        auto add = expr::define_binary(
            mul,
            expr::Ops{"+", expr::Op::add},
            expr::Ops{"-", expr::Op::sub});
        auto eq = expr::define_binary(
            add,
            expr::Ops{"==", expr::Op::equal});
        auto and_ = expr::define_binary(
            eq,
            expr::Ops{"&&", expr::Op::and_});
        auto exp = expr::define_binary(
            and_,
            expr::Ops{"||", expr::Op::or_});
        auto call = expr::define_callexpr<string, vector>(exp);
        auto prim = expr::define_primitive<string>(call);
        ph = expr::make_replacement(seq, expr::define_bracket(prim, exp));
        auto st = expr::define_command_struct<string, vector>(exp, true);
        auto parser = expr::define_set<string, vector>(st, false, false, "program");
        return parser;
    }
    struct CompileContext {
        size_t pos = 0;
        utils::wrap::string buffer;
        utils::wrap::string err;
        utils::wrap::vector<expr::Expr*> errstack;
        utils::wrap::hash_map<utils::wrap::string, expr::Expr*> defs;
        void write(auto&&... v) {
            helper::append(buffer, v...);
        }

        bool error(auto&&... v) {
            helper::append(err, v);
            return false;
        }

        void pushstack(auto&&... v) {
            (errstack.push_back(v)...);
        }
    };

    bool compile(expr::Expr* expr, CompileContext& ctx);
}  // namespace pscmpl
