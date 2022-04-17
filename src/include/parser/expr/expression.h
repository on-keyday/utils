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
#include "../../helper/equal.h"

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

            struct ErrorStack {
               private:
                void* ptr = nullptr;
                void (*pusher)(void* ptr, const char* type, const char* msg, const char* loc, size_t line, size_t begin, size_t end, void* aditional) = nullptr;
                void (*poper)(void* ptr, size_t index) = nullptr;
                size_t (*indexer)(void* ptr) = nullptr;

                template <class T>
                static void push_fn(void* ptr, const char* type, const char* msg, const char* loc, size_t line, size_t begin, size_t end, void* aditional) {
                    static_cast<T*>(ptr)->push(type, msg, loc, line, begin, end, aditional);
                }

                template <class T>
                static void pop_fn(void* ptr, size_t index) {
                    static_cast<T*>(ptr)->pop(index);
                }

                template <class T>
                static size_t index_fn(void* ptr) {
                    return static_cast<T*>(ptr)->index();
                }

               public:
                ErrorStack() {}
                ErrorStack(const ErrorStack& b)
                    : ptr(b.ptr),
                      pusher(b.pusher),
                      poper(b.poper),
                      indexer(b.indexer) {}

                template <class T>
                ErrorStack(T& pb)
                    : ptr(std::addressof(pb)),
                      pusher(push_fn<T>),
                      poper(pop_fn<T>),
                      indexer(index_fn<T>) {}

                bool push(const char* type, const char* msg, size_t begin, size_t end, const char* loc, size_t line, void* additional = nullptr) {
                    if (pusher) {
                        pusher(ptr, type, msg, loc, line, begin, end, additional);
                    }
                    return false;
                }

                bool pop(size_t index) {
                    if (poper) {
                        poper(ptr, index);
                    }
                    return true;
                }

                size_t index() {
                    return indexer(ptr);
                }
            };

            template <class Str>
            struct StackObj {
                Str type;
                Str msg;
                Str loc;
                size_t line;
                size_t begin;
                size_t end;
                void* additional;
            };

            template <class String, template <class...> class Vec>
            struct Errors {
                Vec<StackObj<String>> stack;
                void push(auto... v) {
                    stack.push_back(StackObj<String>{v...});
                }

                void pop(size_t) {}
                size_t index() {
                    return 0;
                }
            };

#define PUSH_ERROR(stack, type, msg, begin, end) stack.push(type, msg, begin, end, __FILE__, __LINE__, nullptr);
#define PUSH_ERRORA(stack, type, msg, begin, end, additional) stack.push(type, msg, begin, end, __FILE__, __LINE__, additional);

            struct StartPos {
                size_t start;
                size_t pos;
                bool reset;
                size_t* ptr;
                constexpr StartPos() {}
                StartPos(StartPos&& p) {
                    ::memcpy(this, &p, sizeof(StartPos));
                    p.reset = false;
                    p.ptr = nullptr;
                }

                bool ok() {
                    reset = false;
                    return true;
                }

                bool fatal() {
                    reset = false;
                    return false;
                }

                bool err() {
                    reset = false;
                    *ptr = start;
                    return false;
                }

                ~StartPos() {
                    if (reset && ptr) {
                        err();
                    }
                }
            };
            template <class T>
            [[nodiscard]] StartPos save(Sequencer<T>& seq) {
                StartPos pos;
                pos.start = seq.rptr;
                pos.reset = true;
                pos.pos = seq.rptr;
                pos.ptr = &seq.rptr;
                return pos;
            }

            template <class T>
            [[nodiscard]] StartPos save_and_space(Sequencer<T>& seq, bool line = true) {
                StartPos pos;
                pos.start = seq.rptr;
                pos.reset = true;
                helper::space::consume_space(seq, line);
                pos.pos = seq.rptr;
                pos.ptr = &seq.rptr;
                return pos;
            }

            template <class T>
            auto bind_space(Sequencer<T>& seq) {
                return [&](bool line = true) {
                    return helper::space::consume_space(seq, line);
                };
            }

            template <class T>
            bool boolean(Sequencer<T>& seq, bool& v, size_t& pos) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (seq.seek_if("true")) {
                    v = true;
                }
                else if (seq.seek_if("false")) {
                    v = false;
                }
                else {
                    return false;
                }
                if (!helper::space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    return false;
                }
                return pos_.ok();
            }

            template <class T, class Int>
            bool integer(Sequencer<T>& seq, Int& v, size_t& pos, int& pf) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (!number::prefix_integer(seq, v, &pf)) {
                    return false;
                }
                if (!helper::space::consume_space(seq, true) &&
                    number::is_alnum(seq.current())) {
                    return false;
                }
                return pos_.ok();
            }

            template <class T, class String>
            bool string(Sequencer<T>& seq, String& str, size_t& pos) {
                auto pos_ = save_and_space(seq);
                if (!escape::read_string(str, seq, escape::ReadFlag::escape)) {
                    return false;
                }
                helper::space::consume_space(seq, true);
                return pos_.ok();
            }

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

            constexpr auto default_filter() {
                return []<class T>(Sequencer<T>& seq) {
                    auto c = seq.current();
                    return number::is_alnum(c) || c == '_';
                };
            }

            template <class String, class T, class Filter = decltype(default_filter())>
            bool variable(Sequencer<T>& seq, String& name, size_t& pos, Filter&& filter = default_filter()) {
                auto pos_ = save_and_space(seq);
                pos = pos_.pos;
                if (!helper::read_whilef<true>(name, seq, [&](auto&&) {
                        return filter(seq);
                    })) {
                    return false;
                }
                return pos_.ok();
            }

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
                    if (primitive<String>(seq, expr)) {
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

            template <class String, template <class...> class Vec>
            struct CallExpr : Expr {
                String name;
                Vec<Expr*> args;

                CallExpr(size_t pos)
                    : Expr(typeCall, pos) {}

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

            template <class String, template <class...> class Vec, class Fn, class Filter = decltype(default_filter())>
            auto define_callexpr(Fn next, Filter filter = default_filter()) {
                return [=]<class T>(Sequencer<T>& seq, Expr*& expr, ErrorStack& stack) {
                    auto space = bind_space(seq);
                    String name;
                    size_t pos;
                    if (!variable(seq, name, pos, filter)) {
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
                            if (!next(seq, expr, stack)) {
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
