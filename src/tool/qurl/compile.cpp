/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "compile.h"
#include "runtime.h"
#include "script.h"
#include <format>
#include <map>
#include <number/prefix.h>

namespace qurl::compile {

    bool compile_int(Env& env, const std::shared_ptr<node::Token>& ival) {
        if (ival->tag != script::k_int) {
            env.error(Error{
                .msg = std::format("expect int token but {} token appear", ival->tag),
                .pos = ival->pos,
            });
            return false;
        }
        std::uint64_t value = 0;
        if (!utils::number::prefix_integer(ival->token, value)) {
            env.error(Error{
                .msg = std::format("int token {} is not parsable as 64 bit integer", ival->token),
                .pos = ival->pos,
            });
            return false;
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::PUSH,
            .value = runtime::Value{.value = value, .pos = ival->pos},
            .pos = ival->pos,
        });
        return true;
    }

    bool compile_ident(Env& env, const std::shared_ptr<node::Token>& ival) {
        if (ival->tag != script::k_ident) {
            env.error(Error{
                .msg = std::format("expect ident token but {} token appear", ival->tag),
                .pos = ival->pos,
            });
            return false;
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::PUSH,
            .value = runtime::Value{.value = runtime::Name{ival->token}, .pos = ival->pos},
            .pos = ival->pos,
        });
        return true;
    }

    bool compile_expr(Env& env, const std::shared_ptr<node::Node>& expr);

    bool compile_call(Env& env, const std::shared_ptr<node::Node>& expr) {
        if (expr->tag != script::g_call || !expr->is_group) {
            env.error(Error{
                .msg = std::format("expect call group but {} appear", expr->tag),
                .pos = expr->pos,
            });
            return false;
        }
        auto call = std::static_pointer_cast<node::Group>(expr);
        for (auto it = call->children.rbegin(); it != call->children.rend(); it++) {
            if (!compile_expr(env, *it)) {
                return false;
            }
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::CALL,
            .value = runtime::Value{.value = call->children.size(), .pos = call->pos},
            .pos = call->pos,
        });
        return true;
    }

    bool compile_func(Env& env, const std::shared_ptr<node::Group>& func) {
        if (func->tag != script::g_func || !func->is_group) {
            env.error(Error{
                .msg = std::format("expect fn group but {} appear", func->tag),
                .pos = func->pos,
            });
            return false;
        }
        size_t i = 0;
        std::vector<runtime::Param> params;
        std::map<std::string, node::Token*> param_set;
        bool vaarg = false;
        for (; i < func->children.size(); i++) {
            auto param = func->children[i];
            if (param->tag != script::k_ident) {
                break;
            }
            auto tok = static_cast<node::Token*>(param.get());
            params.push_back(runtime::Param{
                .param = tok->token,
                .pos = tok->pos,
            });
            if (auto found = param_set.find(tok->token); found != param_set.end()) {
                env.error(Error{
                    .msg = std::format("parameter `{}` is duplicated", tok->token),
                    .pos = tok->pos,
                });
                env.error(Error{
                    .msg = std::format("parameter `{}` is defined here", tok->token),
                    .pos = found->second->pos,
                });
                return false;
            }
            param_set[tok->token] = tok;
        }
        if (i < func->children.size() &&
            func->children[i]->tag == script::k_token &&
            static_cast<node::Token*>(func->children[i].get())->token == "...") {
            vaarg = true;
            i++;
        }
        if (i + 1 != func->children.size()) {
            env.error(Error{
                .msg = std::format("expect a block but not. maybe bug?"),
                .pos = func->pos,
            });
            return false;
        }
        auto block = func->children[i];
        if (block->tag != script::g_block || !block->is_group) {
            env.error(Error{
                .msg = std::format("expect block group but `{}` detected", block->tag),
                .pos = block->pos,
            });
        }
        auto ablock = std::static_pointer_cast<node::Group>(block);
        Env inner;
        if (!compile(inner, ablock)) {
            env.errors = std::move(inner.errors);
            return false;
        }
        auto rfun = std::make_shared<runtime::Func>();
        rfun->istrs = std::move(inner.istrs);
        if (!rfun->istrs.size() ||
            (rfun->istrs.back().op != runtime::Op::RET &&
             rfun->istrs.back().op != runtime::Op::RET_NONE)) {
            rfun->istrs.push_back(runtime::Instruction{
                .op = runtime::Op::RET_NONE,
                .pos = func->pos,
            });
        }
        rfun->params = std::move(params);
        rfun->pos = func->pos;
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::PUSH,
            .value = runtime::Value{.value = std::move(rfun), .pos = func->pos},
            .pos = func->pos,
        });
        return true;
    }

    bool map_binary_to_instruction(Env& env, node::Token* sym) {
#define SWITCH() \
    if (false) { \
    }
#define PUSH(OP) env.istrs.push_back(runtime::Instruction{ \
    .op = OP,                                              \
    .pos = sym->pos,                                       \
});
#define CASE(V, op)             \
    else if (sym->token == V) { \
        PUSH(op)                \
        return true;            \
    }
#define DEFAULT() else

        SWITCH()
        CASE("+", runtime::Op::ADD)
        CASE("-", runtime::Op::SUB)
        CASE("*", runtime::Op::MUL)
        CASE("/", runtime::Op::DIV)
        CASE("%", runtime::Op::MOD)
        CASE("==", runtime::Op::EQ)
        DEFAULT() {
            env.error(Error{
                .msg = std::format("unexpected symbol `{}`", sym->token),
                .pos = sym->pos,
            });
            return false;
        }
#undef SWITCH
#undef CASE
#undef DEFAULT
    }

    bool compile_expr(Env& env, const std::shared_ptr<node::Node>& expr) {
        if (!expr->is_group) {
            auto ival = std::static_pointer_cast<node::Token>(expr);
            if (ival->tag == script::k_ident) {
                return compile_ident(env, ival);
            }
            else if (ival->tag == script::k_int) {
                return compile_int(env, ival);
            }
            env.error(Error{
                .msg = std::format("unexpected {} token `{}` appear", expr->tag, ival->token),
                .pos = expr->pos,
            });
            return false;
        }
        auto gr = std::static_pointer_cast<node::Group>(expr);
        if (gr->tag == script::g_func) {
            return compile_func(env, gr);
        }
        if (!compile_expr(env, gr->children[0])) {
            return false;
        }
        for (size_t i = 1; i < gr->children.size();) {
            auto br = gr->children[i];
            if (br->tag == script::g_call) {
                if (!compile_call(env, br)) {
                    return false;
                }
                i++;
                continue;
            }
            if (br->tag == script::k_token) {
                i++;
                if (i >= gr->children.size()) {
                    env.error(Error{
                        .msg = std::format("expect child more but not"),
                        .pos = br->pos,
                    });
                    return false;
                }
                if (!compile_expr(env, gr->children[i])) {
                    return false;
                }
                if (!map_binary_to_instruction(env, static_cast<node::Token*>(br.get()))) {
                    return false;
                }
                i++;
                continue;
            }
            env.error(Error{
                .msg = std::format("unexpected token or group `{}` appear", expr->tag),
                .pos = expr->pos,
            });
            return false;
        }
        return true;
    }

    bool compile_assign(Env& env, const std::shared_ptr<node::Group>& assign) {
        if (assign->children.size() != 3) {
            env.error(Error{
                .msg = std::format("expect 3 token for children but {} token appear", assign->children.size()),
                .pos = assign->pos,
            });
            return false;
        }
        auto ident = assign->children[0];
        auto sym = assign->children[1];
        auto value = assign->children[2];
        if (ident->is_group) {
            env.error(Error{
                .msg = std::format("currently multiple assignment is not supported but {} group detected", ident->tag),
                .pos = ident->pos,
            });
            return false;
        }
        if (ident->tag != script::k_ident) {
            env.error(Error{
                .msg = std::format("expect ident token but {} token detected", ident->tag),
                .pos = ident->pos,
            });
            return false;
        }
        if (sym->tag != script::k_token || sym->is_group) {
            env.error(Error{
                .msg = std::format("expect token `:=` or `=` but {} appaer", sym->tag),
                .pos = sym->pos,
            });
            return false;
        }
        auto asym = static_cast<node::Token*>(sym.get());
        auto var_name = static_cast<node::Token*>(ident.get());
        if (!compile_expr(env, value)) {
            return false;
        }
        if (asym->token == ":=") {
            env.istrs.push_back(runtime::Instruction{
                .op = runtime::Op::ALLOC,
                .value = runtime::Value{.value = runtime::Name{var_name->token}, .pos = var_name->pos},
                .pos = assign->pos,
            });
        }
        else if (asym->token == "=") {
            env.istrs.push_back(runtime::Instruction{
                .op = runtime::Op::ASSIGN,
                .value = runtime::Value{.value = runtime::Name{var_name->token}, .pos = var_name->pos},
                .pos = assign->pos,
            });
        }
        else {
            env.error(Error{
                .msg = std::format("expect token `:=` or `=` but {} appaer", asym->token),
                .pos = sym->pos,
            });
            return false;
        }
        return true;
    }

    bool compile_ret(Env& env, const std::shared_ptr<node::Group>& ret) {
        if (ret->children.size() == 2) {
            if (!compile_expr(env, ret->children[1])) {
                return false;
            }
            env.istrs.push_back(runtime::Instruction{
                .op = runtime::Op::RESOLVE,
                .pos = ret->pos,
            });
            env.istrs.push_back(runtime::Instruction{
                .op = runtime::Op::RET,
                .pos = ret->pos,
            });
        }
        else {
            env.istrs.push_back(runtime::Instruction{
                .op = runtime::Op::RET_NONE,
                .pos = ret->pos,
            });
        }
        return true;
    }

    bool compile_if(Env& env, const std::shared_ptr<node::Group>& node) {
        if (node->tag != script::g_if || !node->is_group) {
            env.error(Error{
                .msg = std::format("expect if group but {} appear", node->tag),
                .pos = node->pos,
            });
            return false;
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::ENTER,
            .pos = node->pos,
        });
        auto expr = node->children[1];
        auto block = node->children[2];
        if (!block->is_group || block->tag != script::g_block) {
            env.error(Error{
                .msg = std::format("expect block group but {} appear", block->tag),
                .pos = block->pos,
            });
            return false;
        }
        auto if_label = env.get_label();
        if (!compile_expr(env, expr)) {
            return false;
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::JMPIF,
            .value = runtime::Label{
                .label = if_label,
                .fwd = false,
            },
            .pos = {block->pos.begin, block->pos.begin + 1},
        });
        Env inner;
        if (!compile(inner, std::static_pointer_cast<node::Group>(block))) {
            env.errors = std::move(inner.errors);
            return false;
        }
        for (auto& istr : inner.istrs) {
            if (istr.op == runtime::Op::RET ||
                istr.op == runtime::Op::RET_NONE) {
                env.istrs.push_back(runtime::Instruction{
                    .op = runtime::Op::LEAVE,
                    .pos = istr.pos,
                });
            }
            env.istrs.push_back(std::move(istr));
        }
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::LABEL,
            .value = if_label,
            .pos = {block->pos.end - 1, block->pos.end},
        });
        env.istrs.push_back(runtime::Instruction{
            .op = runtime::Op::LEAVE,
            .pos = {block->pos.end - 1, block->pos.end},
        });
        return true;
    }

    bool compile(Env& env, const std::shared_ptr<node::Group>& node) {
        for (auto& istr : node->children) {
            if (!istr->is_group) {
                env.error(Error{
                    .msg = std::format("non-group element {} in block", istr->tag),
                    .pos = istr->pos,
                });
                return false;
            }
            auto g = std::static_pointer_cast<node::Group>(istr);
            if (g->tag == script::g_assign) {
                if (!compile_assign(env, g)) {
                    return false;
                }
            }
            else if (g->tag == script::g_expr) {
                if (!compile_expr(env, g)) {
                    return false;
                }
                // pop evaluated value from stack
                env.istrs.push_back(runtime::Instruction{
                    .op = runtime::Op::POP,
                    .pos = g->pos,
                });
            }
            else if (g->tag == script::g_return) {
                if (!compile_ret(env, g)) {
                    return false;
                }
            }
            else if (g->tag == script::g_if) {
                if (!compile_if(env, g)) {
                    return false;
                }
            }
            else {
                env.error(Error{
                    .msg = std::format("unexpected group `{}`", istr->tag),
                    .pos = istr->pos,
                });
                return false;
            }
        }
        return true;
    }
}  // namespace qurl::compile
