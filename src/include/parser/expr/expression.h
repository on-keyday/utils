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
                PushBacker(const PushBacker& b)
                    : ptr(b.ptr), push_back_(b.push_back_) {}

                template <class T>
                PushBacker(T& pb)
                    : ptr(std::addressof(pb)), push_back_(push_back_fn<T>) {}

                void push_back(std::uint8_t c) {
                    push_back_(ptr, c);
                }
            };

            template <class T>
            bool boolean(Sequencer<T>& seq, bool& v, size_t& pos) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                pos = seq.rptr;
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
            bool integer(Sequencer<T>& seq, Int& v, size_t& pos, int& pf) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                pos = seq.rptr;
                if (!number::prefix_integer(seq, v, &pf)) {
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
            bool string(Sequencer<T>& seq, String& str, size_t& pos) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                pos = seq.rptr;
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

            template <class String>
            struct VarExpr : Expr {
                VarExpr(String&& n, size_t pos)
                    : name(std::move(n)), Expr("variable", pos) {}
                String name;

                bool stringify(PushBacker pb) const override {
                    helper::append(pb, name);
                    return true;
                }
            };

            template <class String, class T>
            bool variable(Sequencer<T>& seq, String& name, size_t& pos) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                pos = seq.rptr;
                if (!helper::read_whilef<true>(name, seq, [](auto&& c) {
                        return number::is_alnum(c) || c == '_' || c == ':';
                    })) {
                    seq.rptr = start;
                    return false;
                }
                return true;
            }

            template <class String, class T>
            bool variable(Sequencer<T>& seq, Expr*& expr) {
                String name;
                size_t pos = 0;
                if (!variable(seq, name, pos)) {
                    return false;
                }
                helper::space::consume_space(seq, true);
                expr = new VarExpr<String>{std::move(name), pos};
                return true;
            }

            struct BoolExpr : Expr {
                bool value;
                BoolExpr(bool v, size_t pos)
                    : value(v), Expr("bool", pos) {}
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
                    : value(v), radix(radix), Expr("integer", pos) {}

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
                    : value(std::move(v)), Expr("string", pos) {}
                bool stringify(PushBacker pb) const override {
                    helper::append(pb, value);
                    return true;
                }
            };

            template <class String, class T>
            bool primitive(Sequencer<T>& seq, Expr*& expr) {
                size_t pos = 0;
                int radix = 0;
                if (bool v = false; boolean(seq, v, pos)) {
                    expr = new BoolExpr{v, pos};
                }
                else if (std::int64_t i = 0; integer(seq, i, pos, radix)) {
                    expr = new IntExpr{i, radix, pos};
                }
                else if (String s; string(seq, s, pos)) {
                    expr = new StringExpr<String>{std::move(s), pos};
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
                equal,
            };

            constexpr bool is_arithmetic(Op op) {
                return op == Op::add || op == Op::sub || op == Op::mul ||
                       op == Op::div || op == Op::mod;
            }

            struct BinExpr : Expr {
                Expr* left;
                Expr* right;
                Op op;
                const char* str;

                BinExpr(size_t pos)
                    : Expr("binary", pos) {}

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
                size_t pos = seq.rptr;
                if (!seq.seek_if(expect)) {
                    return 0;
                }
                auto bexpr = new BinExpr(pos);
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

            inline auto define_replacement(PlaceHolder*& place) {
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

            struct WrapExpr : Expr {
                Expr* child;

                WrapExpr(const char* ty, size_t pos)
                    : Expr(ty, pos) {}

                Expr* index(size_t i) const override {
                    return i == 0 ? child : nullptr;
                }

                ~WrapExpr() {
                    delete child;
                }
            };

            template <class Fn1, class Fn2>
            auto define_brackets(Fn1 next, Fn2 recur, const char* wrap = nullptr) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr) {
                    size_t start = seq.rptr;
                    utils::helper::space::consume_space(seq, true);
                    size_t pos = seq.rptr;
                    if (seq.consume_if('(')) {
                        if (!recur(seq, expr)) {
                            return false;
                        }
                        utils::helper::space::consume_space(seq, true);
                        if (!seq.consume_if(')')) {
                            delete expr;
                            expr = nullptr;
                            return false;
                        }
                        if (wrap) {
                            auto wexpr = new WrapExpr{wrap, pos};
                            wexpr->child = expr;
                            expr = wexpr;
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

                CallExpr(size_t pos)
                    : Expr("call", pos) {}

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
                    auto space = [&] {
                        helper::space::consume_space(seq, true);
                    };
                    space();
                    String name;
                    size_t pos = seq.rptr;
                    if (!variable(seq, name, pos)) {
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
                        space();
                        auto cexpr = new CallExpr<String, Vec>{pos};
                        cexpr->name = std::move(name);
                        cexpr->args = std::move(vexpr);
                        expr = cexpr;
                    }
                    else {
                        auto var = new VarExpr<String>{std::move(name), pos};
                        expr = var;
                    }
                    return true;
                };
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils