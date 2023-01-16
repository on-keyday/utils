/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"

#define NOT_ERROR(expr, err) \
    bool err = false;        \
    if (expr; err) {         \
        return false;        \
    }                        \
    else
#define CHANGE_SCOPE(result, expr)       \
    bool result = false;                 \
    {                                    \
        RuntimeScope scope;              \
        scope.static_scope = node->owns; \
        scope.parent = current;          \
        current = &scope;                \
        result = expr;                   \
        current = scope.parent;          \
    }
namespace minilang {
    namespace runtime {
        bool Interpreter::eval_for(Node* node, RuntimeValue& value, bool as_if) {
            auto len = length(node->children);
            auto cond_repeat = [&](Node* inits, Node* cond, Node* rep, Node* block) {
                auto repeat = [&] {
                    while (true) {
                        bool brek = as_if;
                        auto func = [&] {
                            NOT_ERROR(auto res = !cond || eval_as_bool(cond, err), err) {
                                if (res == false) {
                                    brek = true;
                                    return true;
                                }
                                if (!walk_node(block, value)) {
                                    return false;
                                }
                            }
                            return true;
                        };
                        CHANGE_SCOPE(res, func())
                        if (!res) {
                            return false;
                        }
                        if (brek) {
                            break;
                        }
                        if (state != BackTo::none) {
                            break;
                        }
                    }
                    if (state == BackTo::loop) {
                        state = BackTo::none;
                    }
                    return true;
                };
                if (inits) {
                    auto with_init = [&] {
                        if (!walk_node(inits, value)) {
                            return false;
                        }
                        return repeat();
                    };
                    CHANGE_SCOPE(res, with_init())
                    return res;
                }
                else {
                    return repeat();
                }
            };
            if (len == 1) {
                auto block = node->children->node[0];
                return cond_repeat(nullptr, nullptr, nullptr, block);
            }
            else if (len == 2) {
                auto cond = node->children->node[0];
                auto block = node->children->node[1];
                return cond_repeat(nullptr, cond, nullptr, block);
            }
            else if (len == 3) {
                auto inits = node->children->node[0];
                auto cond = node->children->node[1];
                auto block = node->children->node[2];
                return cond_repeat(inits, cond, nullptr, block);
            }
            else if (len == 4) {
                auto inits = node->children->node[0];
                auto cond = node->children->node[1];
                auto rep = node->children->node[2];
                auto block = node->children->node[3];
                return cond_repeat(inits, cond, rep, block);
            }
            return false;
        }

        bool Interpreter::eval_if(Node* node, RuntimeValue& value) {
            auto len = length(node->children);
            if (len == 1) {
                error("if statement requires least one conditional", node);
                return false;
            }
            if (len == 4) {
                error("if statement requires no repeat", node);
                return false;
            }
            return eval_for(node, value, true);
        }

        bool Interpreter::walk_node(Node* node, RuntimeValue& value) {
            if (is(node->expr, "for")) {
                return eval_for(node, value, false);
            }
            else if (is(node->expr, "if")) {
                return eval_if(node, value);
            }
            else if (is(node->expr, "let")) {
                auto let = static_cast<expr::LetExpr<wrap::string>*>(node->expr);
                if (!let->init_expr) {
                    error("initial expr must be required", node);
                    return false;
                }
                auto init = node->child(1);
                RuntimeValue value;
                if (!eval_expr(value, init)) {
                    return false;
                }
                auto var = current->define_var(let->idname, node);
                if (!var) {
                    error("failed to define symbol", node);
                    return false;
                }
                var->value = value;
                return true;
            }
            else if (is(node->expr, "program") || is(node->expr, "block")) {
                if (length(node->children)) {
                    for (auto ch : node->children->node) {
                        if (!walk_node(ch, value)) {
                            return false;
                        }
                        if (state != BackTo::none) {
                            break;
                        }
                    }
                }
                return true;
            }
            else if (is(node->expr, "func")) {
                auto signode = node->child(0);
                auto signature = static_cast<expr::CallExpr<wrap::string, wrap::vector>*>(signode->expr);
                auto func = current->define_var(signature->name, node);
                if (!func) {
                    error("failed to define function", node);
                    return false;
                }
                func->value.emplace(Function{
                    .node = node,
                });
                return true;
            }
            else if (is(node->expr, "return")) {
                auto val = node->child(0);
                if (!eval_expr(value, val)) {
                    return false;
                }
                state = BackTo::func;
                return true;
            }
            else if (is(node->expr, "expr_stat")) {
                if (!eval_expr(value, node->child(0))) {
                    return false;
                }
                return true;
            }
            error("unknown node", node);
            return false;
        }

        bool Interpreter::eval_binary(RuntimeValue& value, Node* node) {
            auto bin = static_cast<expr::BinExpr*>(node->expr);
            if (expr::is_logical(bin->op)) {
                if (!eval_expr(value, node->children->node[0])) {
                    return false;
                }

                // short-circuit evaluation
                if (value.get_bool()) {
                    if (bin->op == expr::Op::or_) {
                        value.emplace(Boolean{
                            .node = node,
                            .value = true,
                        });
                        return true;
                    }
                }
                else {
                    if (bin->op == expr::Op::and_) {
                        value.emplace(Boolean{
                            .node = node,
                            .value = false,
                        });
                        return true;
                    }
                }

                if (!eval_expr(value, node->children->node[1])) {
                    return false;
                }
                value.emplace(Boolean{
                    .node = node,
                    .value = value.get_bool(),
                });
                return true;
            }
            else if (expr::is_arithmetic(bin->op)) {
                RuntimeValue other;
                if (!eval_expr(value, node->child(0))) {
                    return false;
                }
                if (!eval_expr(other, node->child(1))) {
                    return false;
                }
                auto filter = value.reflect(other);
                auto binab = [&](OpFilter op, const char* symbol, auto fn) {
                    if (!any(filter & op)) {
                        number::Array<char, 50, true> tmp;
                        helper::appends(tmp, "operator ", symbol, " is not surpported here");
                        error(tmp.c_str(), node);
                        return false;
                    }
                    auto i1 = value.as_int();
                    auto i2 = other.as_int();
                    if (op == OpFilter::div || op == OpFilter::mod) {
                        if (*i2 == 0) {
                            error("div by zero error!", node);
                            return false;
                        }
                    }
                    value.emplace(Integer{
                        .node = node,
                        .value = fn(*i1, *i2),
                    });
                    return true;
                };
                if (bin->op == expr::Op::add) {
                    if (!any(filter & OpFilter::add)) {
                        error("operator + is not supported on here", node);
                        return false;
                    }
                    if (auto i1 = value.as_int()) {
                        auto i2 = other.as_int();
                        value.emplace(Integer{
                            .node = node,
                            .value = *i1 + *i2,
                        });
                    }
                    else {
                        auto s1 = value.as_str();
                        auto s2 = other.as_str();
                        value.emplace(String{
                            .node = node,
                            .value = *s1 + *s2,
                        });
                    }
                    return true;
                }
                else if (bin->op == expr::Op::sub) {
                    return binab(OpFilter::sub, "-", [](auto a, auto b) { return a - b; });
                }
                else if (bin->op == expr::Op::mul) {
                    return binab(OpFilter::mul, "*", [](auto a, auto b) { return a * b; });
                }
                else if (bin->op == expr::Op::div) {
                    return binab(OpFilter::div, "/", [](auto a, auto b) { return a / b; });
                }
                else if (bin->op == expr::Op::mod) {
                    return binab(OpFilter::mod, "%", [](auto a, auto b) { return a % b; });
                }
            }
            else if (expr::is_compare(bin->op)) {
                RuntimeValue other;
                if (!eval_expr(value, node->child(0))) {
                    return false;
                }
                if (!eval_expr(other, node->child(1))) {
                    return false;
                }
                auto compare = [&](auto fn) {
                    if (value.value.index() != other.value.index()) {
                        value.emplace(Boolean{
                            .node = node,
                            .value = false,
                        });
                        return true;
                    }
                    if (auto b1 = value.as_bool()) {
                        auto b2 = other.as_bool();
                        value.emplace(Boolean{
                            .node = node,
                            .value = fn(*b1, *b2),
                        });
                    }
                    else if (auto i1 = value.as_int()) {
                        auto i2 = other.as_int();
                        value.emplace(Boolean{
                            .node = node,
                            .value = fn(*i1, *i2),
                        });
                    }
                    else if (auto s1 = value.as_str()) {
                        auto s2 = other.as_str();
                        value.emplace(Boolean{
                            .node = node,
                            .value = fn(*s1, *s2),
                        });
                    }
                    else if (auto f1 = value.as_function()) {
                        auto f2 = other.as_function();
                        value.emplace(Boolean{
                            .node = node,
                            .value = fn(f1, f2),
                        });
                    }
                    else if (auto v1 = value.as_builtin()) {
                        auto v2 = other.as_builtin();
                        value.emplace(Boolean{
                            .node = node,
                            .value = fn(v1, v2),
                        });
                    }
                    else {
                        error("unexpected type", node);
                        return false;
                    }
                    return true;
                };
                if (bin->op == expr::Op::equal) {
                    return compare([](auto&& a, auto&& b) { return a == b; });
                }
                else if (bin->op == expr::Op::not_equal) {
                    return compare([](auto&& a, auto&& b) { return a != b; });
                }
                else if (bin->op == expr::Op::less || bin->op == expr::Op::greater_equal) {
                    return compare([](auto&& a, auto&& b) { return a < b; });
                }
                else if (bin->op == expr::Op::greater || bin->op == expr::Op::less_equal) {
                    return compare([](auto&& a, auto&& b) { return a > b; });
                }
                else {
                    error("this operator not surpported", node);
                    return false;
                }
                return true;
            }
            else if (bin->op == expr::Op::assign) {
                RuntimeValue place;
                if (!eval_expr(place, node->child(1))) {
                    return false;
                }
                if (!eval_expr(value, node->child(0))) {
                    return false;
                }
                if (!value.relvar) {
                    error("left of = must have reference to variable", node);
                    return false;
                }
                value.relvar->value = place;
                return true;
            }
            error("this operator is not supported", node);
            return false;
        }

        bool Interpreter::eval_expr(RuntimeValue& value, Node* node) {
            if (is(node->expr, "expr_stat")) {
                return eval_expr(value, node->child(0));
            }
            else if (is(node->expr, "binary")) {
                return eval_binary(value, node);
            }
            else if (is(node->expr, "fcall")) {
                auto target = node->child(0);
                if (!eval_expr(value, target)) {
                    return false;
                }
                auto fnode = value.as_function();
                if (!fnode) {
                    auto builtin = value.as_builtin();
                    if (builtin) {
                        return invoke_builtin(builtin, node);
                    }
                    error("expect function but not", node);
                    return false;
                }
                return call_function(fnode, node, value);
            }
            else if (is(node->expr, expr::typeVariable)) {
                auto var = resolve(node);
                if (!var) {
                    return false;
                }
                value = var->value;
                value.relvar = var;
            }
            else if (is(node->expr, expr::typeString)) {
                auto str = static_cast<expr::StringExpr<wrap::string>*>(node->expr);
                value.emplace(String{
                    .node = node,
                    .value = str->value,
                });
            }
            else if (is(node->expr, expr::typeInt)) {
                auto integer = static_cast<expr::IntExpr*>(node->expr);
                value.emplace(Integer{
                    .node = node,
                    .value = integer->value,
                });
            }
            else if (is(node->expr, expr::typeBool)) {
                auto boolean = static_cast<expr::BoolExpr*>(node->expr);
                value.emplace(Boolean{
                    .node = node,
                    .value = boolean->value,
                });
            }
            else {
                error("unexpected node for expression", node);
                return false;
            }
            return true;
        }

        RuntimeVar* Interpreter::resolve(Node* varnode) {
            auto var = static_cast<expr::VarExpr<wrap::string>*>(varnode->expr);
            if (!varnode->symbol) {
                varnode->symbol = resolve_symbol(current->static_scope, "var", var->name);
                if (!varnode->symbol) {
                    auto builtin = root.find_var(var->name, rootnode->belongs);
                    if (builtin && builtin->value.as_builtin()) {
                        return builtin;
                    }
                    error("symbol not resolved", varnode);
                    return nullptr;
                }
                if (varnode->symbol->kind != ScopeKind::global) {
                    error("unresolved symbol must be global but local symbol is here", varnode);
                    return nullptr;
                }
                varnode->resolved_at = 2;
            }
            auto instance = current->find_var(var->name, varnode->symbol);
            if (!instance) {
                error("no symbol instance found", varnode);
                return nullptr;
            }
            return instance;
        }

        bool Interpreter::eval_as_bool(Node* node, bool& err) {
            RuntimeValue value;
            if (!eval_expr(value, node)) {
                err = true;
            }
            return value.get_bool();
        }

        bool Interpreter::call_function(Node* fnode, Node* node, RuntimeValue& value) {
            auto instance = fnode->child(0);  // include func name
            auto block = fnode->child(1);     // include block
            auto argument = node->children;
            size_t i = 1;
            RuntimeScope scope;
            scope.static_scope = fnode->owns;
            scope.parent = &root;
            if (argument) {
                for (; i < argument->node.size(); i++) {
                    auto arg = instance->child(i - 1);
                    if (!arg) {
                        error("too many arguments are provided", node);
                        error("defined here", fnode);
                        return false;
                    }
                    if (!eval_expr(value, argument->node[i])) {
                        return false;
                    }
                    auto argexpr = static_cast<expr::LetExpr<wrap::string>*>(arg->expr);
                    auto def = scope.define_var(argexpr->idname, arg);
                    if (!def) {
                        error("multiple define detected", arg);
                        return false;
                    }
                    def->value = std::move(value);
                }
            }
            if (instance->child(i)) {
                error("too few arguments are provided", node);
                error("defined here", fnode);
                return false;
            }
            auto save = current;
            current = &scope;
            auto res = walk_node(block, value);
            current = save;
            state = BackTo::none;
            value.relvar = nullptr;
            return res;
        }

        bool Interpreter::add_builtin(wrap::string key, void* obj, BuiltInProc proc) {
            auto var = root.define_var(key, nullptr);
            if (!var) {
                return false;
            }
            var->value.emplace(BuiltIn{
                .node = nullptr,
                .name = key,
                .object = obj,
                .proc = proc,

            });
            return true;
        }

        bool Interpreter::invoke_builtin(BuiltIn* builtin, Node* node) {
            auto argument = node->children;
            size_t i = 1;
            RuntimeEnv env;
            if (argument) {
                for (; i < argument->node.size(); i++) {
                    RuntimeValue value;
                    if (!eval_expr(value, argument->node[i])) {
                        return false;
                    }
                    env.args.push_back(std::move(value));
                }
            }
            builtin->proc(builtin->object, builtin->name.c_str(), &env);
            return !env.abort;
        }
    }  // namespace runtime
}  // namespace minilang
