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

    struct VarDefExpr : expr::Expr {
        utils::wrap::string name;
        expr::Expr* init = nullptr;
        VarDefExpr(utils::wrap::string&& vn, size_t pos)
            : name(std::move(vn)), expr::Expr("vardef", pos) {}

        bool stringify(expr::PushBacker pb) const override {
            hlp::append(pb, name);
            return true;
        }

        expr::Expr* index(size_t i) const override {
            return i == 0 ? init : nullptr;
        }
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
        auto or_ = expr::define_binary(
            and_,
            expr::Ops{"||", expr::Op::or_});
        auto exp = expr::define_assignment(
            or_,
            expr::Ops{"=", expr::Op::assign});
        auto call = expr::define_callexpr<string, vector>(exp, expr::typeCall, false, [](auto& v) {
            constexpr auto default_ = expr::default_filter();
            return v.current() == ':' || default_(v);
        });
        auto prim = expr::define_primitive<string>(call);
        auto cmds = expr::define_command_each(exp);
        auto fn = [cmds, exp]<class U>(utils::Sequencer<U>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            size_t start = seq.rptr;
            hlp::space::consume_space(seq, true);
            if (seq.seek_if("var")) {
                if (!hlp::space::consume_space(seq, true)) {
                    seq.rptr = start;
                    goto OUT;
                }
                string name;
                size_t pos;
                if (!expr::variable(seq, name, pos)) {
                    return false;
                }
                if (!exp(seq, expr, stack)) {
                    return false;
                }
                auto vexpr = new VarDefExpr(std::move(name), pos);
                vexpr->init = expr;
                expr = vexpr;
                return true;
            }
        OUT:
            return cmds(seq, expr, stack);
        };
        auto st = expr::define_block<string, vector>(fn, true);
        auto anonymous_blcok = expr::define_block<string, vector>(fn, false, "block");
        auto parser = expr::define_block<string, vector>(
            [st, &state]<class U>(utils::Sequencer<U>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
                auto pos = expr::save_and_space(seq);
                auto read_str_and_pack = [&](const char* type) {
                    hlp::space::consume_space(seq, true);
                    string v;
                    size_t strpos = 0;
                    bool fatal = false;
                    if (!expr::string(seq, v, strpos, fatal)) {
                        PUSH_ERROR(stack, type, "expect string but not", strpos, seq.rptr)
                        return pos.fatal();
                    }
                    auto wexpr = new expr::WrapExpr{type, pos.pos};
                    wexpr->child = new expr::StringExpr<string>{std::move(v), strpos};
                    expr = wexpr;
                    return pos.ok();
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
                else if (seq.seek_if("sysimport")) {
                    return read_str_and_pack("sysimport");
                }
                pos.err();
                return st(seq, expr, stack);
            },
            false, "program", 0);
        auto br = expr::define_brackets(prim, exp, "brackets");
        auto recursive = [br, anonymous_blcok]<class U>(utils::Sequencer<U>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            size_t start = seq.rptr;
            utils::helper::space::consume_space(seq, true);
            if (seq.match("{")) {
                return anonymous_blcok(seq, expr, stack);
            }
            seq.rptr = start;
            return br(seq, expr, stack);
        };
        ph = expr::make_replacement(seq, recursive);
        return parser;
    }

    enum class CompileFlag {
        none = 0,
        with_main = 0x1,
    };

    DEFINE_ENUM_FLAGOP(CompileFlag)

    struct CompileContext {
        CompileFlag flag = CompileFlag::none;
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

    struct Stack {
        Stack* parent = nullptr;
        utils::wrap::vector<Stack> children;

        Stack* child() {
            children.push_back({});
            return &children.back();
        }
    };

    void verbose_parse(expr::Expr* expr);
    void parse_msg(const char* msg);

    bool compile(expr::Expr* expr, CompileContext& ctx);
}  // namespace pscmpl
