/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <parser/expr/command_expr.h>
#include <parser/expr/jsoncvt.h>
#include <wrap/light/string.h>
#include <wrap/light/vector.h>
#include <wrap/light/hash_map.h>
#include <random>
using namespace utils::parser;

namespace pscmpl {
    namespace hlp = utils::helper;

    struct ProgramState {
        size_t package = 0;
        utils::wrap::vector<expr::Expr*> locs;
    };

    template <class T>
    auto define_parser(const utils::Sequencer<T>& seq, expr::PlaceHolder*& ph, ProgramState& state) {
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
        auto st = expr::define_command_struct<string, vector>(exp, true);
        auto anonymous_blcok = expr::define_command_struct<string, vector>(exp, false, "block");
        auto parser = expr::define_set<string, vector>(
            [st, &state]<class U>(utils::Sequencer<U>& seq, expr::Expr*& expr) {
                size_t start = seq.rptr;
                hlp::space::consume_space(seq, true);
                size_t pos = seq.rptr;
                auto read_str_and_pack = [&](const char* type) {
                    hlp::space::consume_space(seq, true);
                    string v;
                    size_t strpos = 0;
                    if (!expr::string(seq, v, strpos)) {
                        return false;
                    }
                    auto wexpr = new expr::WrapExpr{type, pos};
                    wexpr->child = new expr::StringExpr<string>{std::move(v), strpos};
                    expr = wexpr;
                    return true;
                };
                if (seq.seek_if("package")) {
                    auto res = read_str_and_pack("package");
                    if (res) {
                        state.package++;
                        state.locs.push_back(expr);
                    }
                    return res;
                }
                else if (seq.seek_if("import")) {
                    return read_str_and_pack("import");
                }
                return st(seq, expr);
            },
            false, false, "program");
        auto br = expr::define_brackets(prim, exp, "brackets");
        auto recursive = [br, anonymous_blcok]<class U>(utils::Sequencer<U>& seq, expr::Expr*& expr) {
            size_t start = seq.rptr;
            utils::helper::space::consume_space(seq, true);
            if (seq.match("{")) {
                return anonymous_blcok(seq, expr);
            }
            seq.rptr = start;
            return br(seq, expr);
        };
        ph = expr::make_replacement(seq, recursive);
        return parser;
    }
    struct CompileContext {
        size_t pos = 0;
        utils::wrap::string buffer;
        utils::wrap::string err;
        utils::wrap::vector<expr::Expr*> errstack;
        utils::wrap::hash_map<utils::wrap::string, expr::Expr*> defs;
        size_t varindex = 0;
        std::random_device dev;
        template <class Out>
        void append_random(Out& out, size_t count = 20) {
            constexpr char index[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            std::uniform_int_distribution<> uni(0, sizeof(index) - 2);
            for (size_t i = 0; i < count; i++)
                out.push_back(index[uni(dev)]);
        }

        void write(auto&&... v) {
            utils::helper::appends(buffer, v...);
        }

        bool error(auto&&... v) {
            utils::helper::appends(err, v...);
            return false;
        }

        void push(expr::Expr* e) {
            errstack.push_back(e);
        }
        void pushstack() {}

        void pushstack(auto&& f, auto&&... v) {
            push(f);
            pushstack(v...);
        }
    };

    void verbose_parse(expr::Expr* expr);
    void parse_msg(const char* msg);

    bool compile(expr::Expr* expr, CompileContext& ctx);
}  // namespace pscmpl
