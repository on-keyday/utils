/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "ipret.h"

namespace combl::ipret {
    bool eval_func(InEnv& env, std::shared_ptr<trvs::Func>& fn) {
        env.call_stack->funcs[fn->name] = std::shared_ptr<trvs::FuncAttr>(fn, &fn->attr);
        return true;
    }

    void collect_comma(std::vector<std::shared_ptr<trvs::Expr>>& args, const std::shared_ptr<trvs::Expr>& expr) {
        if (!expr) {
            return;
        }
        if (expr->type == trvs::ExprType::bin) {
            auto bin = static_cast<trvs::Binary*>(expr.get());
            if (bin->op == ",") {
                collect_comma(args, bin->left);
                collect_comma(args, bin->right);
                return;
            }
        }
        args.push_back(expr);
    }

    bool eval_closure(InEnv& env, std::shared_ptr<trvs::FuncExpr>& fn) {
        trvs::collect_closure_var_candidate(fn->attr.block, [](auto&& s) {

        });
        return false;
    }

    bool eval_expr(InEnv& env, std::shared_ptr<trvs::Expr>& expr, bool as_callee = false) {
        if (expr->type == trvs::ExprType::bin) {
            auto bin = std::static_pointer_cast<trvs::Binary>(expr);
            if (!eval_expr(env, bin->left)) {
                return false;
            }
            if (!eval_expr(env, bin->right)) {
                return false;
            }
            env.call_stack->eval.istrs.push_back({istr::Op{bin}});
            return true;
        }
        else if (expr->type == trvs::ExprType::fn) {
            auto fn = static_cast<trvs::FuncExpr*>(expr.get());
            env.call_stack->eval.istrs.push_back({Value{&fn->attr}});
            return true;
        }
        else if (expr->type == trvs::ExprType::call) {
            auto call = std::static_pointer_cast<trvs::CallExpr>(expr);
            if (!eval_expr(env, call->callee, true)) {
                return false;
            }
            env.call_stack->eval.istrs.push_back({istr::Callee{call}});
            std::vector<std::shared_ptr<trvs::Expr>> args;
            collect_comma(args, call->arguments);
            for (auto& arg : args) {
                if (!eval_expr(env, arg)) {
                    return false;
                }
                env.call_stack->eval.istrs.push_back({istr::Arg{}});
            }
            env.call_stack->eval.istrs.push_back({istr::Call{}});
            return true;
        }
        else if (expr->type == trvs::ExprType::ident) {
            auto ident = static_cast<trvs::Ident*>(expr.get());
            auto l = env.lookup(ident->ident, as_callee);
            auto access = [&](auto& ref) {
                if constexpr (!std::is_same_v<decltype(ref), std::monostate&>) {
                    env.call_stack->eval.istrs.push_back({Value{ref}});
                }
            };
            if (l.index() == 0) {
                env.err.push_back({"identifier `" + ident->ident + "` is not defined", ident->pos});
                return false;
            }
            std::visit(access, l);
            return true;
        }
        else if (expr->type == trvs::ExprType::integer) {
            auto num = static_cast<trvs::Integer*>(expr.get());
            std::uint64_t value = 0;
            if (!utils::number::parse_integer(num->num, value)) {
                env.err.push_back({"integer like `" + num->num + "` is not parsable. maybe too large", num->pos});
                return false;
            }
            env.call_stack->eval.istrs.push_back({Value{value}});
            return true;
        }
        else if (expr->type == trvs::ExprType::str) {
            auto str = static_cast<trvs::String*>(expr.get());
            std::string value;
            value = str->literal;
            value.erase(0, 1);
            value.pop_back();
            if (str->str_type != trvs::StrType::raw) {
                std::string tmp;
                if (!utils::escape::unescape_str(value, tmp)) {
                    env.err.push_back({"string like " + str->literal + " cannot unescape. maybe invalid escape sequence", str->pos});
                }
                value = std::move(tmp);
            }
            env.call_stack->eval.istrs.push_back({Value{std::move(value)}});
            return true;
        }
        else if (expr->type == trvs::ExprType::par) {
            auto par = static_cast<trvs::ParExpr*>(expr.get());
            return eval_expr(env, par->inner, as_callee);
        }
        else if (expr->type == trvs::ExprType::for_) {
            env.call_stack->eval.istrs.push_back({istr::Loop{std::static_pointer_cast<trvs::For>(expr)}});
            return true;
        }
        env.err.push_back({"unsupported expression", expr->expr_range()});
        return false;
    }

    bool eval_let(InEnv& env, std::shared_ptr<trvs::Let>& let) {
        auto& value = env.call_stack->vars[let->ident];
        if (let->init) {
            if (!eval_expr(env, let->init)) {
                return false;
            }
            env.call_stack->eval.istrs.push_back({istr::Init{std::shared_ptr<Value>(env.call_stack, &value), let}});
        }
        return true;
    }

    bool eval_return(InEnv& env, trvs::Return& ret) {
        if (ret.value) {
            if (!eval_expr(env, ret.value)) {
                return false;
            }
        }
        env.call_stack->eval.istrs.push_back({istr::Ret{ret.return_pos}});
        return true;
    }

    bool eval_builtin_callback(InEnv& env, istr::Callback& cb, istr::BuiltinRentrant& call) {
        if (cb.callee == nullptr) {
            env.err.push_back({"builtin function requires callback but callee is not set. user bug", call.node->expr_range()});
            return false;
        }
        istr::CallWait new_cw;
        new_cw.args = std::move(cb.args);
        new_cw.callee = cb.callee;
        new_cw.node = call.node;
        env.call_stack->eval.call.push_back(std::move(new_cw));
        call.count++;
        // execute callback then retntrant builtin
        env.call_stack->eval.istrs.push_front({std::move(call)});  // rentrant
        env.call_stack->eval.istrs.push_front({istr::Call{}});     // exec callback
        return true;
    }

    enum class Ops {
        jump,
        fail,
        ok,
        suspend,
        exit,
    };

    Ops eval_builtin_call_rentrant(InEnv& env, istr::BuiltinRentrant& call) {
        auto builtin = call.callee;
        if (!builtin->eval_builtin) {
            env.err.push_back({"builtin function has no evalution method", call.node->callee->expr_range()});
            return Ops::fail;
        }
        istr::Callback cb;
        auto ev = builtin->eval_builtin(builtin->ctx, cb, call.ret, call.args, call.count);
        if (ev == istr::EvalResult::fail) {
            env.err.push_back({"builtin function call failed. user bug", call.node->expr_range()});
            return Ops::fail;
        }
        if (ev == istr::EvalResult::do_callback) {
            return eval_builtin_callback(env, cb, call) ? Ops::jump : Ops::fail;
        }
        else if (ev == istr::EvalResult::suspend) {
            call.count++;
            env.call_stack->eval.istrs.push_front({std::move(call)});  // for next entrant
            return Ops::suspend;
        }
        else if (ev == istr::EvalResult::exit) {
            return Ops::exit;  // exit interpreter
        }
        env.call_stack->eval.value.push_back(std::move(call.ret));  // set return value
        return Ops::ok;
    }

    Ops eval_builtin_call(InEnv& env, istr::CallWait& call) {
        istr::BuiltinRentrant rent;
        rent.args = std::move(call.args);
        rent.callee = std::get<istr::BuiltinFunc*>(call.callee);
        rent.count = 0;
        rent.node = call.node;
        return eval_builtin_call_rentrant(env, rent);
    }

    Ops eval_call(InEnv& env, istr::CallWait& call) {
        if (std::holds_alternative<istr::BuiltinFunc*>(call.callee)) {
            return eval_builtin_call(env, call);
        }
        auto func_callee = std::get<trvs::FuncAttr*>(call.callee);
        if (!func_callee->block) {
            env.err.push_back({"callee has no definition. only declaration", call.node->callee->expr_range()});
            env.err.push_back({"callee is declared here", func_callee->fn_pos});
            return Ops::fail;
        }
        if (func_callee->args.size() != call.args.size()) {
            std::string str = "expect " + utils::number::to_string<std::string>(func_callee->args.size()) +
                              " but " + utils::number::to_string<std::string>(call.args.size()) + " passed.";
            env.err.push_back({"arguments count is not matched to parameters count. " + str,
                               trvs::Pos{call.node->begin_pos.begin, call.node->end_pos.end}});
            env.err.push_back({"callee is defined here", func_callee->fn_pos});
            return Ops::fail;
        }
        auto func_call_stack = std::make_shared<Stack>();
        func_call_stack->parent = env.call_stack;
        env.call_stack->child = func_call_stack;
        func_call_stack->func = func_callee;
        func_call_stack->current_scope = func_callee->block;
        for (auto i = 0; i < call.args.size(); i++) {
            func_call_stack->vars[func_callee->args[i].name] = std::move(call.args[i]);
        }
        env.call_stack = func_call_stack;  // call
        return Ops::jump;
    }

    std::shared_ptr<Stack> find_function_stack(InEnv& env) {
        auto s = env.call_stack;
        while (s) {
            if (s->func) {
                return s;
            }
        }
        return nullptr;
    }

    bool eval_ret(InEnv& env, trvs::Pos pos) {
        auto s = find_function_stack(env);
        if (!s) {
            env.err.push_back({"`return` at non function scope", pos});
            return false;
        }
        Value retval;
        if (env.call_stack->eval.value.size()) {
            retval = std::move(env.call_stack->eval.value.back());
            env.call_stack->eval.value.pop_back();
        }
        auto next = s->parent.lock();
        if (!next) {
            env.err.push_back({"no parent scope exists. BUG!!", pos});
            return false;
        }
        next->eval.value.push_back(std::move(retval));  // set return value
        next->child = nullptr;                          // free object
        env.call_stack = std::move(next);               // ret
        return true;
    }

    std::shared_ptr<Value> resolve_weak_Value(InEnv& env, std::weak_ptr<Value> w, trvs::Pos pos) {
        auto lock = w.lock();
        if (!lock) {
            env.err.push_back({"reference to expired object. BUG!!", pos});
            return nullptr;
        }
        return lock;
    }

    bool unwrap_Value(InEnv& env, Value& value) {
        while (std::holds_alternative<std::weak_ptr<Value>>(value.value)) {
            auto res = resolve_weak_Value(env, std::get<std::weak_ptr<Value>>(value.value), {});
            if (!res) {
                return false;
            }
            value.value = res->value;
        }
        return true;
    }

    bool eval_bin_op(InEnv& env, std::shared_ptr<trvs::Binary>& bin, std::vector<istr::Value>& stack) {
        if (stack.size() < 2) {
            env.err.push_back({"expect least 2 stack value but only " + utils::number::to_string<std::string>(stack.size()) + " value exists. BUG!", bin->op_pos});
            return false;
        }
        auto right = std::move(stack.back());
        stack.pop_back();
        auto left = std::move(stack.back());
        stack.pop_back();
        if (!unwrap_Value(env, left) || !unwrap_Value(env, right)) {
            return false;
        }
        auto do_op = [&](auto&& cb) {
            {
                std::uint64_t lval = 0, rval = 0;
                if (left.as_uint(lval) && right.as_uint(rval)) {
                    return cb(lval, rval);
                }
            }
            {
                std::int64_t lval, rval = 0;
                if (left.as_int(lval) && right.as_int(rval)) {
                    return cb(lval, rval);
                }
            }
            return false;
        };
        auto invalid_value = [&](const char* msg = "invalid binary expression") {
            env.err.push_back({std::string(msg) + ". values are `" + left.to_string() + " " + bin->op + " " + right.to_string() + "`", bin->expr_range()});
            return false;
        };
        if (bin->op == "+") {
            if (do_op([&](auto l, auto r) { stack.push_back(Value{l + r});return true; })) {
                return true;
            }
            {
                auto lval = left.as_string();
                auto rval = right.as_string();
                if (lval && rval) {
                    stack.push_back(Value{*lval + *rval});
                    return true;
                }
            }
            return invalid_value();
        }
        else if (bin->op == "-") {
            return do_op([&](auto l, auto r) {stack.push_back(Value{std::int64_t(l) - std::int64_t(r)});return true; }) || invalid_value();
        }
        else if (bin->op == "*") {
            return do_op([&](auto l, auto r) {stack.push_back(Value{l*r});return true; }) || invalid_value();
        }
        else if (bin->op == "/") {
            bool div_z = false;
            return do_op([&](auto l, auto r) {
                       if (r == 0) {
                           div_z = true;
                           return false;
                       }
                       stack.push_back(Value{l / r});
                       return true;
                   }) ||
                   invalid_value(div_z ? "div by zero" : "invalid binary expression");
        }
        env.err.push_back({"unsupported operator `" + bin->op + "`",
                           bin->op_pos});
        return false;
    }

    Ops eval_ops(InEnv& env) {
        auto& stack = env.call_stack->eval.value;
        auto& eval_stack = env.call_stack->eval.istrs;
        while (eval_stack.size()) {
            auto& m = env.call_stack->eval.istrs.front().istr;
            if (std::holds_alternative<Value>(m)) {
                stack.push_back(std::move(std::get<Value>(m)));
            }
            else if (std::holds_alternative<istr::Op>(m)) {
                if (!eval_bin_op(env, std::get<istr::Op>(m).node, stack)) {
                    return Ops::fail;
                }
            }
            else if (std::holds_alternative<istr::Callee>(m)) {
                env.call_stack->eval.call.push_back(istr::CallWait{});
                auto& b = env.call_stack->eval.call.back();
                b.node = std::get<istr::Callee>(m).call;
                auto callee = std::move(stack.back());
                stack.pop_back();
                while (std::holds_alternative<std::weak_ptr<Value>>(callee.value)) {
                    auto res = resolve_weak_Value(env, std::get<std::weak_ptr<Value>>(callee.value), {});
                    if (!res) {
                        return Ops::fail;
                    }
                    callee.value = res->value;
                }
                if (!unwrap_Value(env, callee)) {
                    return Ops::fail;
                }
                if (std::holds_alternative<trvs::FuncAttr*>(callee.value)) {
                    b.callee = std::get<trvs::FuncAttr*>(callee.value);
                }
                else if (std::holds_alternative<istr::BuiltinFunc*>(callee.value)) {
                    b.callee = std::get<istr::BuiltinFunc*>(callee.value);
                }
                else {
                    env.err.push_back({"callee is not callable", b.node->callee->expr_range()});
                    return Ops::fail;
                }
            }
            else if (std::holds_alternative<istr::Arg>(m)) {
                env.call_stack->eval.call.back().args.push_back(std::move(stack.back()));
                stack.pop_back();
            }
            else if (std::holds_alternative<istr::Call>(m)) {
                env.call_stack->eval.istrs.pop_front();  // save eval stack
                auto call = std::move(env.call_stack->eval.call.back());
                env.call_stack->eval.call.pop_back();
                const auto ret = eval_call(env, call);
                if (ret != Ops::ok) {
                    return ret;
                }
                continue;
            }
            else if (std::holds_alternative<istr::Init>(m)) {
                auto& assign = std::get<istr::Init>(m);
                auto lock = resolve_weak_Value(env, assign.ptr, assign.node->let_pos);
                if (!lock) {
                    return Ops::fail;
                }
                *lock = std::move(stack.back());
                stack.pop_back();
                stack.push_back(Value{lock});
            }
            else if (std::holds_alternative<istr::Ret>(m)) {
                if (!eval_ret(env, std::get<istr::Ret>(m).retpos)) {
                    return Ops::fail;
                }
                return Ops::jump;
            }
            else if (std::holds_alternative<istr::BuiltinRentrant>(m)) {
                auto val = std::move(std::get<istr::BuiltinRentrant>(m));
                eval_stack.pop_front();
                auto ret = eval_builtin_call_rentrant(env, val);
                if (ret != Ops::ok) {
                    return ret;
                }
                continue;
            }
            eval_stack.pop_front();
        }
        stack.clear();
        eval_stack.clear();
        return Ops::ok;
    }

    Ops eval_current(InEnv& env) {
        auto ops = eval_ops(env);
        if (ops != Ops::ok) {
            return ops;
        }
        auto& stats = env.call_stack->current_scope->statements;
        for (auto i = env.call_stack->stat_index; i < stats.size(); i++) {
            auto& op = stats[i].stat;
            if (std::holds_alternative<std::shared_ptr<trvs::Func>>(op)) {
                if (!eval_func(env, std::get<std::shared_ptr<trvs::Func>>(op))) {
                    return Ops::fail;
                }
            }
            else if (std::holds_alternative<std::shared_ptr<trvs::Let>>(op)) {
                if (!eval_let(env, std::get<std::shared_ptr<trvs::Let>>(op))) {
                    return Ops::fail;
                }
            }
            else if (std::holds_alternative<std::shared_ptr<trvs::Expr>>(op)) {
                if (!eval_expr(env, std::get<std::shared_ptr<trvs::Expr>>(op))) {
                    return Ops::fail;
                }
            }
            else if (std::holds_alternative<trvs::Return>(op)) {
                if (!eval_return(env, std::get<trvs::Return>(op))) {
                    return Ops::fail;
                }
            }
            else if (op.index() == 0) {
                continue;
            }
            else {
                return Ops::fail;
            }
            env.call_stack->stat_index = i + 1;  // save next for call
            auto ops = eval_ops(env);
            if (ops != Ops::ok) {
                return ops;
            }
        }
        return Ops::ok;
    }

    bool eval(InEnv& env) {
        if (!env.global_scope) {
            return false;
        }
        if (!env.call_stack) {
            env.call_stack = std::make_shared<Stack>();
            env.call_stack->current_scope = env.global_scope;
            env.stack_root = env.call_stack;
        }
        while (true) {
            auto ops = eval_current(env);
            if (ops == Ops::fail) {
                return false;
            }
            if (ops == Ops::suspend) {
                return true;
            }
            if (ops == Ops::jump) {
                continue;
            }
            if (ops == Ops::exit) {
                break;
            }
            auto next = env.call_stack->parent.lock();
            if (next) {
                env.call_stack = std::move(next);
                continue;
            }
            break;
        }
        env.call_stack = nullptr;
        env.stack_root = nullptr;
        env.err.clear();
        return true;
    }
}  // namespace combl::ipret
