/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// expression - simple expression
#pragma once

#include "simple_parse.h"
#include "../../helper/equal.h"
#include "interface.h"
#include "simple_parse.h"

namespace utils {
    namespace parser {
        namespace expr {
            constexpr auto typeExpr = "expr";
            constexpr auto typeBool = "bool";
            constexpr auto typeInt = "integer";
            constexpr auto typeString = "string";
            constexpr auto typeVariable = "variable";
            constexpr auto typeCall = "call";
            constexpr auto typeBinary = "binary";

            struct Expr {
               protected:
                const char* type_ = typeExpr;
                size_t pos_ = 0;

               public:
                Expr(const char* ty, size_t p)
                    : type_(ty), pos_(p) {}

                virtual Expr* index(size_t index) const {
                    return nullptr;
                }

                const char* type() const {
                    return type_;
                }

                size_t pos() const {
                    return pos_;
                }

                virtual bool as_int(std::int64_t& val) {
                    return false;
                }
                virtual bool as_bool(bool& val) {
                    return false;
                }

                virtual bool stringify(PushBacker pb) const {
                    return false;
                }

                virtual ~Expr() {}
            };

            inline bool is(expr::Expr* p, const char* ty) {
                return p && helper::equal(p->type(), ty);
            }

            template <class String>
            struct VarExpr : Expr {
                VarExpr(String&& n, size_t pos)
                    : name(std::move(n)), Expr(typeVariable, pos) {}
                String name;

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, name);
                    return true;
                }
            };

            template <class String, class T, class Filter = decltype(default_filter())>
            bool variable(Sequencer<T>& seq, Expr*& expr, Filter&& filter = default_filter()) {
                String name;
                size_t pos = 0;
                if (!variable(seq, name, pos, filter)) {
                    return false;
                }
                helper::space::consume_space(seq, true);
                expr = new VarExpr<String>{std::move(name), pos};
                return true;
            }

            struct BoolExpr : Expr {
                bool value;
                BoolExpr(bool v, size_t pos)
                    : value(v), Expr(typeBool, pos) {}
                bool stringify(PushBacker pb) const override {
                    helper::append(pb, value ? "true" : "false");
                    return true;
                }
                bool as_bool(bool& val) override {
                    val = value;
                    return true;
                }

                bool as_int(std::int64_t& val) override {
                    val = value ? 1 : 0;
                    return true;
                }
            };

            struct IntExpr : Expr {
                std::int64_t value;
                int radix = 10;

                IntExpr(std::int64_t v, int radix, size_t pos)
                    : value(v), radix(radix), Expr(typeInt, pos) {}

                bool stringify(PushBacker pb, int rd) const {
                    number::append_prefix(pb, rd);
                    number::to_string(pb, value, rd);
                    return true;
                }

                bool stringify(PushBacker pb) const override {
                    return stringify(pb, radix);
                }

                bool as_bool(bool& val) override {
                    val = value ? true : false;
                    return true;
                }

                bool as_int(std::int64_t& val) override {
                    val = value;
                    return true;
                }
            };

            template <class String>
            struct StringExpr : Expr {
                String value;
                StringExpr(String&& v, size_t pos)
                    : value(std::move(v)), Expr(typeString, pos) {}
                bool stringify(PushBacker pb) const override {
                    helper::append(pb, value);
                    return true;
                }
            };

            template <class String, class T>
            bool primitive(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                size_t pos = 0;
                int radix = 0;
                bool fatal = false;
                if (bool v = false; boolean(seq, v, pos)) {
                    expr = new BoolExpr{v, pos};
                }
                else if (std::int64_t i = 0; integer(seq, i, pos, radix)) {
                    expr = new IntExpr{i, radix, pos};
                }
                else if (String s; string(seq, s, pos, fatal)) {
                    expr = new StringExpr<String>{std::move(s), pos};
                }
                else {
                    if (fatal) {
                        PUSH_ERROR(stack, "string", "failed to unescape string", pos, pos);
                    }
                    return false;
                }
                return true;
            }

            enum class Op {
                add,
                sub,
                and_,
                or_,
                mul,
                div,
                mod,
                assign,
                equal,
                not_equal,
                less,
                greater,
                less_equal,
                greater_equal,
                three_way_compare,
            };

            constexpr bool is_arithmetic(Op op) {
                return op == Op::add || op == Op::sub || op == Op::mul ||
                       op == Op::div || op == Op::mod;
            }

            constexpr bool is_logical(Op op) {
                return op == Op::and_ || op == Op::or_;
            }

            constexpr bool is_compare(Op op) {
                return op == Op::equal || op == Op::less || op == Op::greater ||
                       op == Op::less_equal || op == Op::greater_equal ||
                       op == Op::three_way_compare;
            }

            struct BinExpr : Expr {
                Expr* left;
                Expr* right;
                Op op;
                const char* str;

                BinExpr(size_t pos)
                    : Expr(typeBinary, pos) {}

                Expr* index(size_t i) const override {
                    if (i == 0) {
                        return left;
                    }
                    if (i == 1) {
                        return right;
                    }
                    return nullptr;
                }

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, str);
                    return true;
                }

                bool get_bothval(bool& lval, bool& rval) {
                    if (!right) {
                        return false;
                    }
                    if (left) {
                        if (!left->as_bool(lval)) {
                            return false;
                        }
                    }
                    if (!right->as_bool(rval)) {
                        return false;
                    }
                    return true;
                }

                bool get_bothval(std::int64_t& lval, std::int64_t& rval) {
                    if (!right) {
                        return false;
                    }
                    if (left) {
                        if (!left->as_int(lval)) {
                            return false;
                        }
                    }
                    if (!right->as_int(rval)) {
                        return false;
                    }
                    return true;
                }

                bool as_bool(bool& val) override {
                    bool lval = false, rval = false;
                    if (!get_bothval(lval, rval)) {
                        return false;
                    }
                    if (op == Op::and_) {
                        val = lval && rval;
                    }
                    else if (op == Op::or_) {
                        val = lval || rval;
                    }

                    else {
                        return false;
                    }
                    return true;
                }

                bool as_int(std::int64_t& val) override {
                    std::int64_t lval = 0, rval = 0;
                    if (!get_bothval(lval, rval)) {
                        return false;
                    }
                    if (op == Op::add) {
                        val = lval + rval;
                    }
                    else if (op == Op::sub) {
                        val = lval - rval;
                    }
                    else if (op == Op::mul) {
                        val = lval * rval;
                    }
                    else if (op == Op::div) {
                        if (rval == 0) {
                            return false;
                        }
                        val = lval / rval;
                    }
                    else if (op == Op::mod) {
                        if (rval == 0) {
                            return false;
                        }
                        val = lval % rval;
                    }
                    else {
                        return false;
                    }
                    return true;
                }

                ~BinExpr() {
                    delete left;
                    delete right;
                }
            };

            template <class T, class Callback, class AddCheck>
            int binexpr(Sequencer<T>& seq, Expr*& expr, const char* expect, Op op, AddCheck&& check, Callback&& next, ErrorStack& stack) {
                size_t pos = seq.rptr;
                if (!seq.seek_if(expect)) {
                    return 0;
                }
                if (!check(seq)) {
                    seq.rptr = pos;
                    return 0;
                }
                auto bexpr = new BinExpr(pos);
                bexpr->left = expr;
                bexpr->right = nullptr;
                bexpr->op = op;
                bexpr->str = expect;
                expr = bexpr;
                Expr* right = nullptr;
                if (!next(seq, right, stack)) {
                    delete bexpr;
                    return -1;
                }
                helper::space::consume_space(seq, true);
                bexpr->right = right;
                return 1;
            }

            template <class T>
            auto make_expect(Sequencer<T>& seq, Expr*& expr, auto&& next, bool& err, ErrorStack& stack) {
                return [&](auto& ops) {
                    if (err) {
                        return false;
                    }
                    auto res = binexpr(
                        seq, expr, ops.expect, ops.op, [&](auto& seq) { return ops.check(seq); }, next, stack);
                    if (res < 0) {
                        err = true;
                        return false;
                    }
                    return res == 1;
                };
            }

            struct Ops {
                const char* expect = nullptr;
                Op op;

                template <class T>
                bool check(Sequencer<T>& seq) const {
                    return true;
                }
            };

            template <class Fn>
            struct OpsCheck {
                const char* expect = nullptr;
                Op op;
                Fn check;
            };

            template <class String, class Filter = decltype(default_filter())>
            auto define_variable(Filter filter = default_filter()) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    return variable<String>(seq, expr, filter);
                };
            }

            template <class String, class Fn = decltype(define_variable<String>())>
            auto define_primitive(Fn custom = define_variable<String>()) {
                return [custom]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    size_t start = seq.rptr;
                    if (primitive<String>(seq, expr, stack)) {
                        return true;
                    }
                    if (!custom(seq, expr, stack)) {
                        PUSH_ERROR(stack, "primitive", "expect primitive value but not", start, seq.rptr)
                        return false;
                    }
                    return true;
                };
            }

            template <class Fn, class... OpSet>
            auto define_binary(Fn next, OpSet... ops) {
                return [next, ops...]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    if (!next(seq, expr, stack)) {
                        return false;
                    }
                    bool err = false;
                    auto expect = make_expect(seq, expr, next, err, stack);
                    bool cont = true;
                    while (cont) {
                        cont = (... || expect(ops));
                    }
                    if (err) {
                        expr = nullptr;
                        return false;
                    }
                    return true;
                };
            }

            struct PlaceHolder {
                virtual bool invoke(void* seq, Expr*& expr, ErrorStack& stack) {
                    return false;
                }
            };

            namespace internal {

                template <class Fn, class T>
                struct Replacement : PlaceHolder {
                    Fn fn;

                    Replacement(Fn&& f)
                        : fn(std::move(f)) {}

                    bool invoke(void* seq, Expr*& expr, ErrorStack& stack) override {
                        return fn(*static_cast<Sequencer<T>*>(seq), expr, stack);
                    }
                };
            }  // namespace internal

            template <class T, class Fn>
            auto make_replacement(const Sequencer<T>&, Fn fn) {
                return new internal::Replacement<Fn, T>(std::move(fn));
            }

            inline auto define_replacement(PlaceHolder*& place) {
                return [&]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    if (!place) {
                        return false;
                    }
                    return place->invoke(&seq, expr, stack);
                };
            }

            template <class Fn, class... OpSet>
            auto define_assignment(Fn next, OpSet... ops) {
                auto fn = [next, ops...]<class T>(auto& self, Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    if (!next(seq, expr, stack)) {
                        return false;
                    }
                    bool err = false;
                    auto expect = make_expect(
                        seq, expr, [&](Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                            return self(self, seq, expr, stack);
                        },
                        err, stack);
                    bool cont = true;
                    while (cont) {
                        cont = (... || expect(ops));
                    }
                    if (err) {
                        expr = nullptr;
                        return false;
                    }
                    return true;
                };
                return [fn]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    return fn(fn, seq, expr, stack);
                };
            }

            struct WrapExpr : Expr {
                Expr* child;

                WrapExpr(const char* ty, size_t pos)
                    : Expr(ty, pos) {}

                Expr* index(size_t i) const override {
                    return i == 0 ? child : nullptr;
                }

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, type_);
                    return true;
                }

                ~WrapExpr() {
                    delete child;
                }
            };

            template <class Fn1, class Fn2>
            auto define_brackets(Fn1 next, Fn2 recur, const char* wrap = nullptr) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto pos = save_and_space(seq);
                    if (seq.consume_if('(')) {
                        if (!recur(seq, expr, stack)) {
                            return pos.fatal();
                        }
                        utils::helper::space::consume_space(seq, true);
                        if (!seq.consume_if(')')) {
                            PUSH_ERROR(stack, "brackets", "expect `)` but not", pos.pos, seq.rptr)
                            delete expr;
                            expr = nullptr;
                            return pos.fatal();
                        }
                        if (wrap) {
                            auto wexpr = new WrapExpr{wrap, pos.pos};
                            wexpr->child = expr;
                            expr = wexpr;
                        }
                        return pos.ok();
                    }
                    pos.err();
                    return next(seq, expr, stack);
                };
            }

            template <class Fn>
            auto define_wrapexpr(const char* type, Fn fn) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    if (!fn(seq, expr, stack)) {
                        return false;
                    }
                    auto wexpr = new WrapExpr{type, expr->pos()};
                    wexpr->child = expr;
                    expr = wexpr;
                    return true;
                };
            }

            template <class String, template <class...> class Vec>
            struct CallExpr : Expr {
                String name;
                Vec<Expr*> args;
                using Expr::Expr;

                Expr* index(size_t i) const override {
                    if (i >= args.size()) {
                        return nullptr;
                    }
                    return args[i];
                }

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, name);
                    return true;
                }

                ~CallExpr() {
                    for (auto v : args) {
                        delete v;
                    }
                }
            };

            template <class T, template <class...> class Vec, class Callback>
            bool brackets_each(Sequencer<T>& seq, Vec<expr::Expr*>& vexpr, ErrorStack& stack, size_t* ppos, bool& err, char beg, char end, Callback&& callback) {
                auto pos = save_and_space(seq);
                if (ppos) {
                    *ppos = pos.pos;
                }
                if (!seq.consume_if('(')) {
                    return false;
                }
                pos.ok();
                auto space = bind_space(seq);
                space();
                auto delexpr = [&]() {
                    for (auto v : vexpr) {
                        delete v;
                    }
                };
                space();
                while (true) {
                    expr::Expr* expr = nullptr;
                    if (seq.consume_if(')')) {
                        break;
                    }
                    if (vexpr.size()) {
                        if (!seq.consume_if(',')) {
                            PUSH_ERROR(stack, "callexpr", "expect `,` but not", pos.pos, seq.rptr);
                            delexpr();
                            err = true;
                            return false;
                        }
                        space();
                    }
                    if (!callback(seq, expr, stack)) {
                        delexpr();
                        err = true;
                        return false;
                    }
                    vexpr.push_back(expr);
                }
                space();
                return true;
            }

            template <class String, template <class...> class Vec, class Fn, class Filter = decltype(default_filter())>
            auto define_callexpr(Fn next, const char* type = typeCall, bool must_not_var = false, char begin = '(', char end = ')', Filter filter = default_filter()) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto space = bind_space(seq);
                    String name;
                    size_t pos;
                    if (!variable(seq, name, pos, filter)) {
                        return false;
                    }
                    space();
                    Vec<expr::Expr*> vexpr;
                    bool err = false;
                    if (auto res = brackets_each(seq, vexpr, stack, nullptr, err, begin, end, next); res || err) {
                        if (!res && err) {
                            return false;
                        }
                        auto cexpr = new CallExpr<String, Vec>{type, pos};
                        cexpr->name = std::move(name);
                        cexpr->args = std::move(vexpr);
                        expr = cexpr;
                    }
                    else {
                        if (must_not_var) {
                            PUSH_ERROR(stack, type, "expect call expr but not", pos, seq.rptr)
                            return false;
                        }
                        auto var = new VarExpr<String>{std::move(name), pos};
                        expr = var;
                    }
                    return true;
                };
            }

            auto define_after(auto prev, auto... after) {
                return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr, ErrorStack& stack) {
                    if (!prev(seq, expr, stack)) {
                        return false;
                    }
                    auto cpy = expr;
                    bool err = false;
                    auto call = [&](auto& fn) {
                        auto start = seq.rptr;
                        auto res = fn(seq, expr, stack);
                        if (!res && seq.rptr != start) {
                            err = true;
                            return true;
                        }
                        return res;
                    };
                    while ((... || call(after))) {
                        if (err) {
                            delete cpy;
                            expr = nullptr;
                            return false;
                        }
                        cpy = expr;
                    }
                    return true;
                };
            }

            template <template <class...> class Vec>
            struct FuncCallExpr : Expr {
                Expr* target;
                Vec<Expr*> args;

                using Expr::Expr;

                Expr* index(size_t i) const override {
                    if (i == 0) return target;
                    if (i - 1 >= args.size()) {
                        return nullptr;
                    }
                    return args[i - 1];
                }

                bool stringify(PushBacker pb) {
                    helper::append(pb, type_);
                    return true;
                }

                ~FuncCallExpr() {
                    delete target;
                    for (auto v : args) {
                        delete v;
                    }
                }
            };

            template <template <class...> class Vec>
            auto define_callop(const char* ty, auto each, char begin = '(', char end = ')') {
                return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr, ErrorStack& stack) {
                    Vec<Expr*> vexpr;
                    bool err;
                    size_t pos = 0;
                    if (!brackets_each(seq, vexpr, stack, &pos, err, begin, end, each)) {
                        return false;
                    }
                    auto cexpr = new FuncCallExpr<Vec>{ty, pos};
                    cexpr->target = expr;
                    cexpr->args = std::move(vexpr);
                    expr = cexpr;
                    return true;
                };
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils
