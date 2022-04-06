/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// expression - simple expression
#pragma once

#include "../../helper/space.h"
#include "../../number/prefix.h"
#include "../../escape/read_string.h"

namespace utils {
    namespace parser {
        namespace expr {

            struct PushBacker {
               private:
                void* ptr;
                void (*push_back_)(void*, std::uint8_t);

                template <class T>
                static void push_back_fn(void* self, std::uint8_t c) {
                    static_cast<T*>(self)->push_back(c);
                }

               public:
                template <class T>
                PushBacker(T& pb)
                    : ptr(std::addressof(pb)), push_back_(push_back_fn<T>) {}

                void push_back(std::uint8_t c) {
                    push_back_(ptr, c);
                }
            };

            template <class T>
            bool boolean(Sequencer<T>& seq, bool& v) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                if (seq.seek_if("true")) {
                    v = true;
                }
                else if (seq.seek_if("false")) {
                    v = false;
                }
                else {
                    seq.rptr = start;
                    return false;
                }
                if (!helper::space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    seq.rptr = start;
                    return false;
                }
                return true;
            }

            template <class T, class Int>
            bool integer(Sequencer<T>& seq, Int& v) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                if (!number::prefix_integer(seq, v)) {
                    seq.rptr = start;
                    return false;
                }
                if (!helper::space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    seq.rptr = start;
                    return false;
                }
                return true;
            }

            template <class T, class String>
            bool string(Sequencer<T>& seq, String& str) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                if (!escape::read_string(str, seq, escape::ReadFlag::escape)) {
                    start = seq.rptr;
                    return false;
                }
                helper::space::consume_space(seq, true);
                return true;
            }

            struct Expr {
               protected:
                const char* type_ = "expr";

               public:
                Expr(const char* ty)
                    : type_(ty) {}

                virtual Expr* index(size_t index) const {
                    return nullptr;
                }

                const char* type() const {
                    return type_;
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

            template <class String>
            struct VarExpr : Expr {
                VarExpr(String&& n)
                    : name(std::move(n)), Expr("variable") {}
                String name;

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, name);
                    return true;
                }
            };

            template <class String, class T>
            bool variable(Sequencer<T>& seq, Expr*& expr) {
                size_t start = seq.rptr;
                String name;
                helper::space::consume_space(seq, true);
                if (!helper::read_whilef<true>(name, seq, [](auto&& c) {
                        return number::is_alnum(c);
                    })) {
                    return false;
                }
                helper::space::consume_space(seq, true);
                expr = new VarExpr<String>{std::move(name)};
                return true;
            }

            struct BoolExpr : Expr {
                bool value;
                BoolExpr(bool v)
                    : value(v), Expr("bool") {}
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

                IntExpr(std::int64_t v)
                    : value(v), Expr("integer") {}

                bool stringify(PushBacker pb) const override {
                    number::to_string(pb, value);
                    return true;
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
                StringExpr(String&& v)
                    : value(std::move(v)), Expr("string") {}
                bool stringify(PushBacker pb) const override {
                    helper::append(pb, value);
                    return true;
                }
            };

            template <class String, class T>
            bool primitive(Sequencer<T>& seq, Expr*& expr) {
                if (bool v = false; boolean(seq, v)) {
                    expr = new BoolExpr{v};
                }
                else if (std::int64_t i = 0; integer(seq, i)) {
                    expr = new IntExpr{i};
                }
                else if (String s; string(seq, s)) {
                    expr = new StringExpr<String>{std::move(s)};
                }
                else {
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
            };

            struct BinExpr : Expr {
                Expr* left;
                Expr* right;
                Op op;
                const char* str;

                BinExpr()
                    : Expr("binary") {}

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

            template <class T, class Callback>
            int binexpr(Sequencer<T>& seq, Expr*& expr, const char* expect, Op op, Callback&& next) {
                if (!seq.seek_if(expect)) {
                    return 0;
                }
                auto bexpr = new BinExpr();
                bexpr->left = expr;
                bexpr->op = op;
                bexpr->str = expect;
                expr = bexpr;
                Expr* right = nullptr;
                if (!next(seq, right)) {
                    delete bexpr;
                    return -1;
                }
                helper::space::consume_space(seq, true);
                bexpr->right = right;
                return 1;
            }

            template <class T>
            auto make_expect(Sequencer<T>& seq, Expr*& expr, auto&& next, bool& err) {
                return [&](const char* expect, Op op) {
                    if (err) {
                        return false;
                    }
                    auto res = binexpr(seq, expr, expect, op, next);
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
            };

            template <class String>
            auto define_variable() {
                return []<class T>(Sequencer<T>& seq, Expr*& expr) {
                    return variable<String>(seq, expr);
                };
            }

            template <class String, class Fn = decltype(define_variable<String>())>
            auto define_primitive(Fn custom = define_variable<String>()) {
                return [custom]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    if (primitive<String>(seq, expr)) {
                        return true;
                    }
                    return custom(seq, expr);
                };
            }

            template <class Fn, class... OpSet>
            auto define_binary(Fn next, OpSet... ops) {
                return [next, ops...]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    if (!next(seq, expr)) {
                        return false;
                    }
                    bool err = false;
                    auto expect = make_expect(seq, expr, next, err);
                    bool cont = true;
                    while (cont) {
                        cont = (... || expect(ops.expect, ops.op));
                    }
                    if (err) {
                        expr = nullptr;
                        return false;
                    }
                    return true;
                };
            }

            struct PlaceHolder {
                virtual bool invoke(void* seq, Expr*& expr) {
                    return false;
                }
            };

            namespace internal {

                template <class Fn, class T>
                struct Replacement : PlaceHolder {
                    Fn fn;

                    Replacement(Fn&& f)
                        : fn(std::move(f)) {}

                    bool invoke(void* seq, Expr*& expr) override {
                        return fn(*static_cast<Sequencer<T>*>(seq), expr);
                    }
                };
            }  // namespace internal

            template <class T, class Fn>
            auto make_replacement(const Sequencer<T>&, Fn fn) {
                return new internal::Replacement<Fn, T>(std::move(fn));
            }

            auto define_replacement(PlaceHolder*& place) {
                return [&]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    if (!place) {
                        return false;
                    }
                    return place->invoke(&seq, expr);
                };
            }

            template <class Fn, class... OpSet>
            auto define_assignment(Fn next, OpSet... ops) {
                auto fn = [next, ops...]<class T>(auto& self, Sequencer<T>& seq, Expr*& expr) {
                    if (!next(seq, expr)) {
                        return false;
                    }
                    bool err = false;
                    auto expect = make_expect(
                        seq, expr, [&](Sequencer<T>& seq, Expr*& expr) {
                            return self(self, seq, expr);
                        },
                        err);
                    bool cont = true;
                    while (cont) {
                        cont = (... || expect(ops.expect, ops.op));
                    }
                    if (err) {
                        expr = nullptr;
                        return false;
                    }
                    return true;
                };
                return [fn]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    return fn(fn, seq, expr);
                };
            }

            template <class Fn1, class Fn2>
            auto define_bracket(Fn1 next, Fn2 recur) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    size_t start = seq.rptr;
                    utils::helper::space::consume_space(seq, true);
                    if (seq.consume_if('(')) {
                        if (!recur(seq, expr)) {
                            return false;
                        }
                        utils::helper::space::consume_space(seq, true);
                        if (!seq.consume_if(')')) {
                            return false;
                        }
                        return true;
                    }
                    seq.rptr = start;
                    return next(seq, expr);
                };
            }

            template <class String, template <class...> class Vec>
            struct CallExpr : Expr {
                String name;
                Vec<Expr*> args;

                CallExpr()
                    : Expr("call") {}

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
            };

            template <class String, template <class...> class Vec, class Fn>
            auto define_callexpr(Fn next) {
                return [next]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    size_t start = 0;
                    auto space = [&] {
                        helper::space::consume_space(seq, true);
                    };
                    space();
                    String name;
                    if (!helper::read_whilef<true>(name, seq, [](auto c) {
                            return number::is_alnum(c);
                        })) {
                        return false;
                    }
                    space();
                    if (seq.consume_if('(')) {
                        Vec<Expr*> vexpr;
                        auto delexpr = [&]() {
                            for (auto v : vexpr) {
                                delete v;
                            }
                        };
                        space();
                        while (true) {
                            if (seq.consume_if(')')) {
                                break;
                            }
                            if (vexpr.size()) {
                                if (!seq.consume_if(',')) {
                                    delexpr();
                                    return false;
                                }
                                space();
                            }
                            if (!next(seq, expr)) {
                                delexpr();
                                return false;
                            }
                            vexpr.push_back(expr);
                            expr = nullptr;
                        }
                        auto cexpr = new CallExpr<String, Vec>{};
                        cexpr->name = std::move(name);
                        cexpr->args = std::move(vexpr);
                        expr = cexpr;
                    }
                    else {
                        auto var = new VarExpr<String>{std::move(name)};
                        expr = var;
                    }
                    return true;
                };
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils