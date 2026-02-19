/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include "game.h"

namespace expr {

    struct Expr : std::enable_shared_from_this<Expr> {
        virtual ~Expr() = default;
        virtual void push(RuntimeState& s) = 0;
        virtual EvalState eval(RuntimeState& s) const = 0;
    };

    EvalState assign_access(RuntimeState& s, std::vector<Access>& access, Value& value);
    std::optional<Value> eval_access(RuntimeState& s, std::vector<Access>& access);

    EvalState invoke_builtin(RuntimeState& s, std::vector<Access>& access, std::vector<Value>& args);

    enum class UnaryOp {
        plus,
        minus,
        not_,
        bit_not,
    };

    struct UnaryExpr : Expr {
        std::shared_ptr<Expr> value;
        UnaryOp op = UnaryOp::plus;

        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.empty()) {
                return s.eval_error(U"スタックが空です(単項演算子)");
            }
            auto v_ = s.stacks.eval.eval_stack.back().eval(s);
            s.stacks.eval.eval_stack.pop_back();
            if (!v_) {
                return EvalState::error;  // already reported
            }
            auto& v = *v_;
            if (v.null()) {
                if (op == UnaryOp::plus) {
                    s.stacks.eval.eval_stack.push_back(EvalValue(nullptr));
                    return EvalState::normal;
                }
                return s.eval_error(U"nullに対して単項演算子を適用することはできません");
            }
            switch (op) {
                case UnaryOp::plus:
                    if (auto n = v.number()) {
                        s.stacks.eval.eval_stack.push_back(EvalValue(*n));
                        return EvalState::normal;
                    }
                    if (auto b = v.boolean()) {
                        s.stacks.eval.eval_stack.push_back(EvalValue(*b));
                        return EvalState::normal;
                    }
                    if (auto str = v.string()) {
                        s.stacks.eval.eval_stack.push_back(EvalValue(*str));
                        return EvalState::normal;
                    }
                    break;
                case UnaryOp::minus:
                    if (auto n = v.number()) {
                        s.stacks.eval.eval_stack.push_back(EvalValue(-*n));
                        return EvalState::normal;
                    }
                    break;
                case UnaryOp::not_:
                    if (auto b = v.boolean()) {
                        b->value = !b->value;
                        s.stacks.eval.eval_stack.push_back(EvalValue(*b));
                        return EvalState::normal;
                    }
                    break;
                case UnaryOp::bit_not:
                    if (auto n = v.number()) {
                        s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(~std::uint64_t(*n))));
                        return EvalState::normal;
                    }
                    break;
            }
            return s.eval_error(U"不正な型に対して単項演算子を適用しました");
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            value->push(s);
        }
    };

    enum class BinaryOp {
        add,
        sub,
        mul,
        div,
        mod,
        eq,
        ne,
        lt,
        le,
        gt,
        ge,
        and_,
        or_,
        bit_and,
        bit_or,
        bit_xor,
        bit_left_shift,
        bit_right_arithmetic_shift,
        bit_right_logical_shift,
    };

    struct BinaryExpr : Expr {
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        BinaryOp op = BinaryOp::add;
        size_t short_circuit = 0;

        EvalState eval_number(RuntimeState& s, std::int64_t left, std::int64_t right) const {
            switch (op) {
                case BinaryOp::add:
                    s.stacks.eval.eval_stack.push_back(EvalValue(left + right));
                    return EvalState::normal;
                case BinaryOp::sub:
                    s.stacks.eval.eval_stack.push_back(EvalValue(left - right));
                    return EvalState::normal;
                case BinaryOp::mul:
                    s.stacks.eval.eval_stack.push_back(EvalValue(left * right));
                    return EvalState::normal;
                case BinaryOp::div:
                    if (right == 0) {
                        return s.eval_error(U"0で割ることはできません");
                    }
                    s.stacks.eval.eval_stack.push_back(EvalValue(left / right));
                    return EvalState::normal;
                case BinaryOp::mod:
                    if (right == 0) {
                        return s.eval_error(U"0で割ることはできません");
                    }
                    s.stacks.eval.eval_stack.push_back(EvalValue(left % right));
                    return EvalState::normal;
                case BinaryOp::eq:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left == right}));
                    return EvalState::normal;
                case BinaryOp::ne:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left != right}));
                    return EvalState::normal;
                case BinaryOp::lt:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left < right}));
                    return EvalState::normal;
                case BinaryOp::le:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left <= right}));
                    return EvalState::normal;
                case BinaryOp::gt:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left > right}));
                    return EvalState::normal;
                case BinaryOp::ge:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left >= right}));
                    return EvalState::normal;
                case BinaryOp::bit_and:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::uint64_t(left) & std::uint64_t(right))));
                    return EvalState::normal;
                case BinaryOp::bit_or:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::uint64_t(left) | std::uint64_t(right))));
                    return EvalState::normal;
                case BinaryOp::bit_xor:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::uint64_t(left) ^ std::uint64_t(right))));
                    return EvalState::normal;
                case BinaryOp::bit_left_shift:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::uint64_t(left) << std::uint64_t(right))));
                    return EvalState::normal;
                case BinaryOp::bit_right_arithmetic_shift:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::int64_t(left) >> std::uint64_t(right))));
                    return EvalState::normal;
                case BinaryOp::bit_right_logical_shift:
                    s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(std::uint64_t(left) >> std::uint64_t(right))));
                    return EvalState::normal;
                default:
                    return s.eval_error(U"不正な二項演算子です(数値)");
            }
        }

        EvalState eval_string(RuntimeState& s, const std::u32string& left, const std::u32string& right) const {
            switch (op) {
                case BinaryOp::add:
                    s.stacks.eval.eval_stack.push_back(EvalValue(left + right));
                    return EvalState::normal;
                case BinaryOp::eq:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left == right}));
                    return EvalState::normal;
                case BinaryOp::ne:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left != right}));
                    return EvalState::normal;
                case BinaryOp::lt:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left < right}));
                    return EvalState::normal;
                case BinaryOp::le:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left <= right}));
                    return EvalState::normal;
                case BinaryOp::gt:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left > right}));
                    return EvalState::normal;
                case BinaryOp::ge:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left >= right}));
                    return EvalState::normal;
                default:
                    return s.eval_error(U"不正な二項演算子です(文字列)");
            }
        }

        EvalState eval_bytes(RuntimeState& s, const Bytes& left, const Bytes& right) const {
            switch (op) {
                case BinaryOp::add:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Bytes{left.data + right.data}));
                    return EvalState::normal;
                case BinaryOp::eq:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left.data == right.data}));
                    return EvalState::normal;
                case BinaryOp::ne:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left.data != right.data}));
                    return EvalState::normal;
                default:
                    return s.eval_error(U"不正な二項演算子です(バイト列)");
            }
        }

        EvalState eval_boolean(RuntimeState& s, bool left, bool right) const {
            switch (op) {
                case BinaryOp::add:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left || right}));
                    return EvalState::normal;
                case BinaryOp::mul:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left && right}));
                case BinaryOp::eq:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left == right}));
                    return EvalState::normal;
                case BinaryOp::ne:
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{left != right}));
                    return EvalState::normal;
                default:
                    return s.eval_error(U"不正な二項演算子です(真偽値)");
            }
        }

        EvalState eval(RuntimeState& s) const override {
            if (op == BinaryOp::and_ || op == BinaryOp::or_) {
                if (s.stacks.eval.eval_stack.size() < 1) {
                    return s.eval_error(U"スタックが空です(短絡評価)");
                }
                auto sh_ = s.stacks.eval.eval_stack.back().eval(s);
                s.stacks.eval.eval_stack.pop_back();
                if (!sh_) {
                    return EvalState::error;  // already reported
                }
                auto sh = sh_->boolean();
                if (!sh) {
                    return s.eval_error(U"真偽値が必要です(短絡評価)");
                }
                bool remove_short_circuit = false;
                if (op == BinaryOp::and_) {
                    remove_short_circuit = !sh->value;
                }
                else {
                    remove_short_circuit = sh->value;
                }
                if (remove_short_circuit) {
                    if (s.stacks.eval.expr_stack.size() < short_circuit) {
                        return s.eval_error(U"スタックが空です(短絡評価)");
                    }
                    for (size_t i = 0; i < short_circuit; i++) {
                        s.stacks.eval.expr_stack.pop_back();
                    }
                    s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{sh->value}));
                }
                return EvalState::normal;
            }
            if (s.stacks.eval.eval_stack.size() < 2) {
                return s.eval_error(U"スタックが空です(二項演算子)");
            }
            auto left_ = s.stacks.eval.eval_stack.back().eval(s);
            s.stacks.eval.eval_stack.pop_back();
            if (!left_) {
                return EvalState::error;  // already reported
            }
            auto right_ = s.stacks.eval.eval_stack.back().eval(s);
            s.stacks.eval.eval_stack.pop_back();
            if (!right_) {
                return EvalState::error;  // already reported
            }
            auto& left = *left_;
            auto& right = *right_;
            auto left_n = left.number();
            auto right_n = right.number();
            auto left_s = left.string();
            auto right_s = right.string();
            auto left_bytes = left.bytes();
            auto right_bytes = right.bytes();
            auto left_b = left.boolean();
            auto right_b = right.boolean();
            auto left_null = left.null();
            auto right_null = right.null();
            if (left_null && right_null) {
                switch (op) {
                    case BinaryOp::eq:
                        s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{true}));
                        return EvalState::normal;
                    case BinaryOp::ne:
                        s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{false}));
                        return EvalState::normal;
                    default:
                        return s.eval_error(U"nullに対して不正な二項演算子を適用しました");
                }
            }
            if (left_null || right_null) {
                switch (op) {
                    case BinaryOp::eq:
                        s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{false}));
                        return EvalState::normal;
                    case BinaryOp::ne:
                        s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{true}));
                        return EvalState::normal;
                    default:
                        return s.eval_error(U"nullに対して不正な二項演算子を適用しました(片方がnull)");
                }
            }
            if (left_n && right_n) {
                return eval_number(s, *left_n, *right_n);
            }
            if (left_s && right_s) {
                return eval_string(s, *left_s, *right_s);
            }
            if (left_bytes && right_bytes) {
                return eval_bytes(s, *left_bytes, *right_bytes);
            }
            if (left_b && right_b) {
                return eval_boolean(s, left_b->value, right_b->value);
            }

            if (left_bytes && right_n) {
                if (op == BinaryOp::mul) {
                    std::string result;
                    for (std::uint64_t i = 0; i < *right_n; i++) {
                        result += left_bytes->data;
                    }
                    s.stacks.eval.eval_stack.push_back(EvalValue(Bytes{result}));
                    return EvalState::normal;
                }
            }
            if (left_s && right_n) {
                if (op == BinaryOp::mul) {
                    std::u32string result;
                    for (std::uint64_t i = 0; i < *right_n; i++) {
                        result += *left_s;
                    }
                    s.stacks.eval.eval_stack.push_back(EvalValue(result));
                    return EvalState::normal;
                }
            }
            return s.eval_error(U"不正な型に対して二項演算子を適用しました");
        }

        void push(RuntimeState& s) override {
            if (op == BinaryOp::and_ || op == BinaryOp::or_) {
                size_t start = s.stacks.eval.expr_stack.size();
                right->push(s);
                size_t end = s.stacks.eval.expr_stack.size();
                short_circuit = end - start;
                s.stacks.eval.expr_stack.push_back(shared_from_this());
                left->push(s);
            }
            else {
                s.stacks.eval.expr_stack.push_back(shared_from_this());
                left->push(s);
                right->push(s);
            }
        }
    };

    struct IdentifierExpr : Expr {
        std::u32string name;
        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(Assignable{name});
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct IntLiteralExpr : Expr {
        std::int64_t value = 0;
        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(EvalValue(value));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct StringLiteralExpr : Expr {
        std::u32string value;
        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(EvalValue(value));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct BytesLiteralExpr : Expr {
        std::string value;

        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(EvalValue(Bytes{value}));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct ArrayLiteralExpr : Expr {
        std::vector<std::shared_ptr<Expr>> elements;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.size() < elements.size()) {
                return s.eval_error(U"スタックが空です(配列リテラル)");
            }
            std::vector<Value> arr;
            for (auto& e : elements) {
                auto v = s.stacks.eval.eval_stack.back().eval(s);
                s.stacks.eval.eval_stack.pop_back();
                if (!v) {
                    return EvalState::error;  // already reported
                }
                arr.push_back(*v);
            }
            s.stacks.eval.eval_stack.push_back(EvalValue(arr));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            for (auto& e : elements) {
                e->push(s);
            }
        }
    };

    struct ObjectLiteralExpr : Expr {
        std::vector<std::pair<std::u32string, std::shared_ptr<Expr>>> elements;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.size() < elements.size()) {
                return s.eval_error(U"スタックが空です(オブジェクトリテラル)");
            }
            std::map<std::u32string, Value> obj;
            for (auto& e : elements) {
                auto v = s.stacks.eval.eval_stack.back().eval(s);
                s.stacks.eval.eval_stack.pop_back();
                if (!v) {
                    return EvalState::error;  // already reported
                }
                obj[e.first] = *v;
            }
            s.stacks.eval.eval_stack.push_back(EvalValue(obj));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            for (auto& e : elements) {
                e.second->push(s);
            }
        }
    };

    struct SpecialObjectExpr : Expr {
        SpecialObject value;

        EvalState eval(RuntimeState& s) const override {
            std::vector<Access> access;
            access.push_back(value);
            s.stacks.eval.eval_stack.push_back(EvalValue(access));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct BoolLiteralExpr : Expr {
        bool value;
        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(EvalValue(Boolean{value}));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct NullLiteralExpr : Expr {
        EvalState eval(RuntimeState& s) const override {
            s.stacks.eval.eval_stack.push_back(EvalValue(nullptr));
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
        }
    };

    struct DotAccessExpr : Expr {
        std::shared_ptr<Expr> left;
        std::u32string right;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.empty()) {
                return s.eval_error(U"スタックが空です(ドットアクセス)");
            }
            auto v = s.stacks.eval.eval_stack.back();
            s.stacks.eval.eval_stack.pop_back();
            if (auto access = v.access()) {
                access->push_back(right);
                s.stacks.eval.eval_stack.push_back(EvalValue(*access));
            }
            else if (auto as = v.assignable()) {
                std::vector<Access> access;
                access.push_back(*as);
                access.push_back(right);
                s.stacks.eval.eval_stack.push_back(EvalValue(access));
            }
            else if (auto o = v.object()) {
                std::vector<Access> access;
                access.push_back(Access(*o));
                access.push_back(right);
            }
            else if (auto a = v.array(); a && right == U"length") {
                s.stacks.eval.eval_stack.push_back(EvalValue(std::int64_t(a->size())));
            }
            else {
                return s.eval_error(U"不正な型にドットアクセスを適用しました");
            }
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            left->push(s);
        }
    };

    struct IndexAccessExpr : Expr {
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.size() < 2) {
                return s.eval_error(U"スタックが空です(インデックスアクセス)");
            }
            auto v = s.stacks.eval.eval_stack.back();
            s.stacks.eval.eval_stack.pop_back();
            auto index = s.stacks.eval.eval_stack.back().eval(s);
            s.stacks.eval.eval_stack.pop_back();
            if (!index) {
                return EvalState::error;  // already reported
            }
            auto n = index->number();
            if (!n) {
                return s.eval_error(U"数値以外のものをインデックスに指定しました");
            }
            if (auto access = v.access()) {
                access->push_back(Access(*n));
                s.stacks.eval.eval_stack.push_back(EvalValue(*access));
            }
            else if (auto as = v.assignable()) {
                std::vector<Access> access;
                access.push_back(*as);
                access.push_back(Access(*n));
                s.stacks.eval.eval_stack.push_back(EvalValue(access));
            }
            else {
                return s.eval_error(U"不正な型にインデックスアクセスを適用しました");
            }
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            left->push(s);
            right->push(s);
        }
    };

    struct AssignExpr : Expr {
        std::shared_ptr<Expr> target;
        std::shared_ptr<Expr> value;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.size() < 2) {
                return s.eval_error(U"スタックが空です(代入文)");
            }
            auto target = s.stacks.eval.eval_stack.back();
            s.stacks.eval.eval_stack.pop_back();
            auto value_ = s.stacks.eval.eval_stack.back().eval(s);
            s.stacks.eval.eval_stack.pop_back();
            if (!value_) {
                return EvalState::error;  // already reported
            }
            auto& value = *value_;
            if (auto access = target.access()) {
                auto err = assign_access(s, *access, value);
                if (err != EvalState::normal) {
                    return err;  // already reported
                }
            }
            else if (auto p = target.assignable()) {
                s.set_variable(p->name, std::move(value));
            }
            else {
                return s.eval_error(U"不正な型に代入を適用しました");
            }
            s.stacks.eval.eval_stack.push_back(target);
            return EvalState::normal;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            target->push(s);
            value->push(s);
        }
    };

    struct CallExpr : Expr {
        std::shared_ptr<Expr> callee;
        std::vector<std::shared_ptr<Expr>> args;
        EvalState eval(RuntimeState& s) const override {
            if (s.stacks.eval.eval_stack.size() < args.size() + 1) {
                return s.eval_error(U"スタックが空です(関数呼び出し)");
            }
            auto func_name = s.stacks.eval.eval_stack.back();
            s.stacks.eval.eval_stack.pop_back();
            std::vector<Value> arg_values;
            for (auto& arg : args) {  // pop from last
                auto v = s.stacks.eval.eval_stack.back().eval(s);
                s.stacks.eval.eval_stack.pop_back();
                if (!v) {
                    return EvalState::error;  // already reported
                }
                arg_values.push_back(*v);
            }
            if (auto access = func_name.access()) {
                if (auto o = (*access)[0].special_object();
                    o &&
                    *o == SpecialObject::Builtin) {
                    return invoke_builtin(s, *access, arg_values);
                }
            }
            auto func_ = func_name.eval(s);
            if (!func_) {
                return EvalState::error;  // already reported
            }
            auto func = func_->func();
            if (!func) {
                return s.eval_error(U"関数ではないものを呼び出しました");
            }
            s.call_stack.push_back(CallStack{
                .back_pos = func->start,  // this will be replaced by caller
                .stacks = std::move(s.stacks),
                .function_scope = func->capture
                                      ? func->capture->clone()
                                      : std::make_shared<Scope>(),
            });
            s.stacks.clear();
            s.stacks.args = std::move(arg_values);
            return EvalState::call;
        }

        void push(RuntimeState& s) override {
            s.stacks.eval.expr_stack.push_back(shared_from_this());
            callee->push(s);
            for (auto& arg : args) {
                arg->push(s);
            }
        }
    };
}  // namespace expr
