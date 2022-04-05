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
                virtual Expr* index(size_t index) {
                    return nullptr;
                }
                virtual bool as_int(std::int64_t& val) {
                    return false;
                }
                virtual bool as_bool(bool& val) {
                    return false;
                }

                virtual bool as_string(void* str) {
                    return false;
                }

                virtual ~Expr() {}
            };

            template <class String>
            struct VarExpr : Expr {
                VarExpr(String&& n)
                    : name(std::move(n)) {}
                String name;
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
                if (!helper::space::consume_space(seq, true)) {
                    return false;
                }
                expr = new VarExpr<String>{std::move(name)};
                return true;
            }

            struct BoolExpr : Expr {
                bool value;
                BoolExpr(bool v)
                    : value(v) {}
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
                    : value(v) {}

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
                    : value(std::move(v)) {}
                bool as_string(void* str) override {
                    auto val = static_cast<String*>(str);
                    *val = value;
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
                else if (variable<String>(seq, expr)) {
                }
                else {
                    return false;
                }
                return true;
            }

            enum class Op {
                add,
                sub,
            };

            struct BinExpr : Expr {
                Expr* left;
                Expr* right;
                Op op;
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
            auto define_primitive() {
                return []<class T>(Sequencer<T>& seq, Expr*& expr) {
                    return primitive<String>(seq, expr);
                };
            }

            template <class Fn, class... OpSet>
            auto define_binary(Fn& next, OpSet&&... ops) {
                return [&, ops...]<class T>(Sequencer<T>& seq, Expr*& expr) {
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
                        return false;
                    }
                    return true;
                };
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils