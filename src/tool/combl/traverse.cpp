/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "traverse.h"
#include <json/iterator.h>
namespace combl::trvs {
    void skip_space(auto& it, const json::JSON& node) {
        while (it != node.aend() && it->at("spaces")) {
            it++;
        }
    }

    const json::JSON* expect(auto& it, const json::JSON& node, const char* s) {
        if (it == node.aend()) {
            return nullptr;
        }
        auto ret = it->at(s);
        if (ret) {
            it++;
        }
        return ret;
    }

    const json::JSON* expect_with_pos(auto& it, const json::JSON& node, const char* s, Pos* pos) {
        if (it == node.aend()) {
            return nullptr;
        }
        auto ret = it->at(s);
        if (ret) {
            if (pos) {
                auto p = it->at("pos");
                if (!p) {
                    return nullptr;
                }
                const json::JSON* b = p->at("begin");
                const json::JSON* e = p->at("end");
                if (!b || !e ||
                    !b->as_number(pos->begin) ||
                    !e->as_number(pos->end)) {
                    return nullptr;
                }
            }
            it++;
        }
        return ret;
    }

    const json::JSON* expect_token(auto& it, const json::JSON& node, const char* tok = nullptr, Pos* pos = nullptr) {
        auto exp = expect_with_pos(it, node, "token", pos);
        if (exp && tok) {
            if (exp->template force_as_string<std::string>() != tok) {
                it--;
                return nullptr;
            }
        }
        return exp;
    }

    bool traverse_expr(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node);
    bool traverse_let(const std::shared_ptr<Scope>& scope, std::shared_ptr<Let>* pass_let, const json::JSON& node);

    bool traverse_attached_block(const std::shared_ptr<Scope>& new_scope, const json::JSON& node);

    bool traverse_fn_expr(const std::shared_ptr<Scope>& scope, std::shared_ptr<FuncExpr>& expr, const json::JSON& node);

    bool traverse_praenthesis(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos begin, end;
        if (!expect_token(it, node, "(", &begin)) {  // (
            return false;
        }
        skip_space(it, node);
        auto inner = expect(it, node, "expr");
        if (!inner) {
            return false;
        }
        if (!traverse_expr(scope, expr, *inner)) {
            return false;
        }
        skip_space(it, node);
        if (!expect_token(it, node, ")", &end)) {  // )
            return false;
        }
        auto par = std::make_shared<ParExpr>();
        par->begin_pos = begin;
        par->inner = std::move(expr);
        par->end_pos = end;
        expr = std::move(par);
        return true;
    }

    bool traverse_if(const std::shared_ptr<Scope>& scope, std::shared_ptr<If>& if_expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        std::shared_ptr<If> cur_if;
        Pos pos;
        auto traverse_if_detail = [&] {
            if (!expect_token(it, node, "if", &pos)) {  // if
                return false;
            }
            if (!cur_if) {
                cur_if = std::make_shared<If>();
                if_expr = cur_if;
            }
            else {
                auto new_if = std::make_shared<If>();
                cur_if->els = new_if;
                cur_if = new_if;
            }
            cur_if->if_pos = pos;
            cur_if->block = std::make_shared<Scope>();
            cur_if->block->parent = scope;
            scope->ident_usage.track.push_back(cur_if->block);
            skip_space(it, node);
            if (auto expr = expect(it, node, "expr")) {
                std::shared_ptr<Expr> val;
                if (!traverse_expr(cur_if->block, val, *expr)) {
                    return false;
                }
                cur_if->first = std::move(val);
            }
            else if (auto let = expect(it, node, "let")) {
                std::shared_ptr<Let> val;
                if (!traverse_let(cur_if->block, &val, *let)) {
                    return false;
                }
                cur_if->first = std::move(val);
            }
            else {
                return false;
            }
            skip_space(it, node);
            if (expect_token(it, node, ";", &pos)) {
                cur_if->delim_pos = pos;
                skip_space(it, node);
                auto expr = expect(it, node, "expr");
                if (!expr) {
                    return false;
                }
                if (!traverse_expr(cur_if->block, cur_if->second, *expr)) {
                    return false;
                }
            }
            skip_space(it, node);
            auto block = expect(it, node, "block");
            if (!block) {
                return false;
            }
            return traverse_attached_block(cur_if->block, *block);
        };
        if (!traverse_if_detail()) {
            return false;
        }
        while (true) {
            skip_space(it, node);
            auto els = expect_token(it, node, "else", &pos);
            if (!els) {
                break;
            }
            cur_if->else_pos = pos;
            skip_space(it, node);
            if (auto block = expect(it, node, "block")) {
                std::shared_ptr<Scope> b = std::make_shared<Scope>();
                b->parent = scope;
                if (!traverse_attached_block(b, *block)) {
                    return false;
                }
                cur_if->els = b;
                break;
            }
            if (!traverse_if_detail()) {
                return false;
            }
        }
        return true;
    }

    bool traverse_for(const std::shared_ptr<Scope>& scope, std::shared_ptr<For>& for_expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "for", &pos)) {  // if
            return false;
        }
        for_expr = std::make_shared<For>();
        for_expr->block = std::make_shared<Scope>();
        for_expr->block->parent = scope;
        scope->ident_usage.track.push_back(for_expr->block);
        const json::JSON* block_ = nullptr;
        do {
            skip_space(it, node);
            block_ = expect(it, node, "block");
            if (block_) {
                break;
            }
            if (auto expr = expect(it, node, "expr")) {
                std::shared_ptr<Expr> val;
                if (!traverse_expr(for_expr->block, val, *expr)) {
                    return false;
                }
                for_expr->first = std::move(val);
            }
            else if (auto let = expect(it, node, "let")) {
                std::shared_ptr<Let> val;
                if (!traverse_let(for_expr->block, &val, *let)) {
                    return false;
                }
                for_expr->first = std::move(val);
            }
            skip_space(it, node);
            block_ = expect(it, node, "block");
            if (block_) {
                break;
            }
            if (!expect_token(it, node, ";", &pos)) {
                return false;
            }
            for_expr->first_delim_pos = pos;
            skip_space(it, node);
            if (auto expr = expect(it, node, "expr")) {
                std::shared_ptr<Expr> val;
                if (!traverse_expr(for_expr->block, val, *expr)) {
                    return false;
                }
                for_expr->second = std::move(val);
            }
            skip_space(it, node);
            block_ = expect(it, node, "block");
            if (block_) {
                break;
            }
            if (!expect_token(it, node, ";", &pos)) {
                return false;
            }
            for_expr->second_delim_pos = pos;
            skip_space(it, node);
            if (auto expr = expect(it, node, "expr")) {
                std::shared_ptr<Expr> val;
                if (!traverse_expr(for_expr->block, val, *expr)) {
                    return false;
                }
                for_expr->third = std::move(val);
            }
            skip_space(it, node);
            block_ = expect(it, node, "block");
            if (!block_) {
                return false;
            }
        } while (0);
        return traverse_attached_block(for_expr->block, *block_);
    }

    bool traverse_primitive(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (auto num = expect_with_pos(it, node, "integer", &pos)) {
            auto i = std::make_shared<Integer>();
            i->num = num->force_as_string<std::string>();
            i->pos = pos;
            expr = i;
            return true;
        }
        else if (auto id = expect_with_pos(it, node, "ident", &pos)) {
            auto i = std::make_shared<Ident>();
            i->ident = id->force_as_string<std::string>();
            i->pos = pos;
            scope->ident_usage.track.push_back({i});
            expr = i;
            return true;
        }
        else if (auto s = expect_with_pos(it, node, "string", &pos)) {
            auto str = std::make_shared<String>();
            str->pos = pos;
            str->literal = s->force_as_string<std::string>();
            str->str_type = StrType::normal;
            expr = str;
            return true;
        }
        else if (auto s = expect_with_pos(it, node, "char", &pos)) {
            auto str = std::make_shared<String>();
            str->pos = pos;
            str->literal = s->force_as_string<std::string>();
            str->str_type = StrType::char_;
            expr = str;
            return true;
        }
        else if (auto s = expect_with_pos(it, node, "raw_string", &pos)) {
            auto str = std::make_shared<String>();
            str->pos = pos;
            str->literal = s->force_as_string<std::string>();
            str->str_type = StrType::raw;
            expr = str;
            return true;
        }
        else if (auto pr = expect(it, node, "praenthesis")) {
            return traverse_praenthesis(scope, expr, *pr);
        }
        else if (auto fn = expect(it, node, "fn_expr")) {
            std::shared_ptr<FuncExpr> fnexpr;
            if (!traverse_fn_expr(scope, fnexpr, *fn)) {
                return false;
            }
            expr = fnexpr;
            return true;
        }
        else if (auto if_ = expect(it, node, "if")) {
            std::shared_ptr<If> ifexpr;
            if (!traverse_if(scope, ifexpr, *if_)) {
                return false;
            }
            expr = ifexpr;
            return true;
        }
        else if (auto if_ = expect(it, node, "for")) {
            std::shared_ptr<For> forexpr;
            if (!traverse_for(scope, forexpr, *if_)) {
                return false;
            }
            expr = forexpr;
            return true;
        }
        return false;
    }

    bool traverse_call(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        std::shared_ptr<Expr> inner;
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "(", &pos)) {
            return false;
        }
        skip_space(it, node);
        if (auto expr = expect(it, node, "expr")) {
            if (!traverse_expr(scope, inner, *expr)) {
                return false;
            }
        }
        skip_space(it, node);
        Pos end;
        if (!expect_token(it, node, ")", &end)) {
            return false;
        }
        auto call = std::make_shared<CallExpr>();
        call->callee = std::move(expr);
        call->begin_pos = pos;
        call->arguments = std::move(inner);
        call->end_pos = end;
        expr = std::move(call);
        return true;
    }

    bool traverse_index(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        std::shared_ptr<Expr> inner;
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "[", &pos)) {
            return false;
        }
        skip_space(it, node);
        if (auto expr = expect(it, node, "expr")) {
            if (!traverse_expr(scope, inner, *expr)) {
                return false;
            }
        }
        skip_space(it, node);
        Pos end;
        if (expect_token(it, node, "]", &end)) {
            auto index = std::make_shared<IndexExpr>();  // example
            index->target = std::move(expr);             // a
            index->begin_pos = pos;                      // [
            index->index = std::move(inner);             // 0
            index->end_pos = end;                        // ]
            expr = std::move(index);
            return true;
        }
        Pos slice_pos;
        if (expect_token(it, node, ":", &slice_pos)) {
            return false;
        }
        skip_space(it, node);
        std::shared_ptr<Expr> slice_end;
        if (auto expr = expect(it, node, "expr")) {
            if (!traverse_expr(scope, slice_end, *expr)) {
                return false;
            }
        }
        if (!expect_token(it, node, "]", &end)) {
            return false;
        }
        auto slice = std::make_shared<SliceExpr>();  // example
        slice->target = std::move(expr);             // a
        slice->begin_pos = pos;                      // [
        slice->begin = std::move(inner);             // 0
        slice->colon_pos = slice_pos;                // :
        slice->end = std::move(slice_end);           // 2
        slice->end_pos = end;                        // ]
        expr = std::move(slice);
        return true;
    }

    bool traverse_postfix(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        auto prim = expect(it, node, "primitive");
        if (!prim) {
            return false;
        }
        if (!traverse_primitive(scope, expr, *prim)) {
            return false;
        }
        while (true) {
            skip_space(it, node);
            if (auto call = expect(it, node, "call")) {
                if (!traverse_call(scope, expr, *call)) {
                    return false;
                }
                continue;
            }
            if (auto index = expect(it, node, "index")) {
                if (!traverse_index(scope, expr, *index)) {
                    return false;
                }
                continue;
            }
            break;
        }
        return true;
    }

    const json::JSON* expect_oneof(auto& it, const json::JSON& node, const char** expected, auto... a) {
        const json::JSON* l = nullptr;
        auto fold = [&](const char* a) {
            l = expect(it, node, a);
            if (l) {
                *expected = a;
            }
            return l != nullptr;
        };
        (... || fold(a));
        return l;
    }

    bool traverse_expr(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        const char* expected = nullptr;
        auto left = expect_oneof(it, node, &expected, "assign", "comma", "equal", "compare", "add", "mul", "postfix");
        if (!left) {
            return traverse_postfix(scope, expr, node);
        }
        if (!traverse_expr(scope, expr, *left)) {
            return false;
        }
        while (true) {
            skip_space(it, node);
            Pos pos;
            auto op = expect_token(it, node, nullptr, &pos);
            if (!op) {
                break;
            }
            skip_space(it, node);
            auto right = expect(it, node, expected);
            if (!right) {
                return false;
            }
            std::shared_ptr<Expr> tmp;
            if (!traverse_expr(scope, tmp, *right)) {
                return false;
            }
            auto new_expr = std::make_shared<Binary>();
            new_expr->op = op->force_as_string<std::string>();
            new_expr->op_pos = pos;
            new_expr->left = std::move(expr);
            new_expr->right = std::move(tmp);
            expr = std::move(new_expr);
        }
        return true;
    }

    bool traverse_fn_args(const std::shared_ptr<Scope>& scope, auto& it, const json::JSON& node, std::vector<Param>& args, std::shared_ptr<Type>& rettype);

    bool traverse_fntype(const std::shared_ptr<Scope>& scope, std::shared_ptr<FnType>& fntype, const json::JSON& node) {
        fntype = std::make_shared<FnType>();
        fntype->type = "fn";
        auto it = node.abegin();
        skip_space(it, node);
        if (!expect_token(it, node, "fn")) {  // fn
            return false;
        }
        return traverse_fn_args(scope, it, node, fntype->args, fntype->base);
    }

    bool traverse_type(const std::shared_ptr<Scope>& scope, std::shared_ptr<Type>& type, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        if (auto token = expect_token(it, node)) {
            type = std::make_shared<Type>();
            type->type = token->force_as_string<std::string>();
            if (type->type == "*") {
                skip_space(it, node);
                auto ty = expect(it, node, "type");
                if (!ty) {
                    return false;
                }
                return traverse_type(scope, type->base, *ty);
            }
            else {
                return false;
            }
        }
        if (auto fn_type = expect(it, node, "fn_type")) {
            std::shared_ptr<FnType> fntype;
            if (!traverse_fntype(scope, fntype, *fn_type)) {
                return false;
            }
            type = fntype;
            return true;
        }
        auto ident = expect(it, node, "type_ident");
        if (!ident) {
            return false;
        }
        type = std::make_shared<Type>();
        type->type = ident->force_as_string<std::string>();
        return true;
    }

    bool traverse_type_annon(const std::shared_ptr<Scope>& scope, std::shared_ptr<Type>& type, const json::JSON& node) {
        auto ait = node.abegin();
        skip_space(ait, node);
        if (!expect(ait, node, "token")) {  // :
            return false;
        }
        skip_space(ait, node);
        auto t = expect(ait, node, "type");
        if (!t) {
            return false;
        }
        if (!traverse_type(scope, type, *t)) {
            return false;
        }
        return true;
    }

    bool traverse_arg(const std::shared_ptr<Scope>& scope, Param& param, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        if (auto ident = expect(it, node, "arg_ident")) {
            param.name = ident->force_as_string<std::string>();
        }
        skip_space(it, node);
        if (auto annon = expect(it, node, "type_annon")) {
            return traverse_type_annon(scope, param.type, *annon);
        }
        return true;
    }

    bool traverse_args(const std::shared_ptr<Scope>& scope, std::vector<Param>& args, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        while (it != node.aend()) {
            expect_token(it, node);  // ,
            auto arg = expect(it, node, "arg");
            if (!arg) {
                return false;
            }
            Param param;
            if (!traverse_arg(scope, param, *arg)) {
                return false;
            }
            args.push_back(std::move(param));
            skip_space(it, node);
        }
        return true;
    }

    bool traverse_fn_args(const std::shared_ptr<Scope>& scope, auto& it, const json::JSON& node, std::vector<Param>& args, std::shared_ptr<Type>& rettype) {
        skip_space(it, node);
        if (!expect_token(it, node, "(")) {  // (
            return false;
        }
        skip_space(it, node);
        if (auto a = expect(it, node, "args")) {
            if (!traverse_args(scope, args, *a)) {
                return false;
            }
        }
        skip_space(it, node);
        if (!expect_token(it, node, ")")) {  // )
            return false;
        }
        skip_space(it, node);
        if (auto r = expect(it, node, "return_type")) {
            auto rit = r->abegin();
            skip_space(rit, *r);
            if (!expect_token(rit, *r, "->")) {  // ->
                return false;
            }
            skip_space(rit, *r);
            auto type = expect(rit, *r, "type");
            if (!type) {
                return false;
            }
            return traverse_type(scope, rettype, *type);
        }
        return true;
    }

    bool traverse_fn_expr(const std::shared_ptr<Scope>& scope, std::shared_ptr<FuncExpr>& expr, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "fn", &pos)) {  // fn
            return false;
        }
        skip_space(it, node);
        expr = std::make_shared<FuncExpr>();
        expr->attr.fn_pos = pos;
        expr->attr.parent = scope;
        if (!traverse_fn_args(scope, it, node, expr->attr.args, expr->attr.rettype)) {
            return false;
        }
        skip_space(it, node);
        auto bl = expect(it, node, "block");
        if (!bl) {
            return false;
        }
        expr->attr.block = std::make_shared<Scope>();
        expr->attr.block->parent = scope;
        return traverse_attached_block(expr->attr.block, *bl);
    }

    bool traverse_fn(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "fn", &pos)) {  // fn
            return false;
        }
        skip_space(it, node);
        auto name = expect(it, node, "fn_name");
        if (!name) {
            return false;
        }
        auto fn = std::make_shared<Func>();
        fn->name = name->force_as_string<std::string>();
        scope->statements.push_back(Stat{fn});
        scope->ident_usage.track.push_back({fn});
        fn->attr.parent = scope;
        fn->attr.fn_pos = pos;
        if (!traverse_fn_args(scope, it, node, fn->attr.args, fn->attr.rettype)) {
            return false;
        }
        skip_space(it, node);
        if (auto bl = expect(it, node, "block")) {
            fn->attr.block = std::make_shared<Scope>();
            fn->attr.block->parent = scope;
            return traverse_attached_block(fn->attr.block, *bl);
        }
        return true;
    }

    bool traverse_init(const std::shared_ptr<Scope>& scope, std::shared_ptr<Expr>& init, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        if (!expect_token(it, node, "=")) {  // =
            return false;
        }
        skip_space(it, node);
        auto expr = expect(it, node, "expr");
        if (!expr) {
            return false;
        }
        if (!traverse_expr(scope, init, *expr)) {
            return false;
        }
        return true;
    }

    bool traverse_let(const std::shared_ptr<Scope>& scope, std::shared_ptr<Let>* pass_let, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "let", &pos)) {  // let
            return false;
        }
        skip_space(it, node);
        auto ident = expect(it, node, "ident");
        if (!ident) {
            return false;
        }
        auto let = std::make_shared<Let>();
        let->ident = ident->force_as_string<std::string>();
        let->let_pos = pos;
        if (auto annon = expect(it, node, "type_annon")) {
            if (!traverse_type_annon(scope, let->type, *annon)) {
                return false;
            }
        }
        if (auto ini = expect(it, node, "init")) {
            if (!traverse_init(scope, let->init, *ini)) {
                return false;
            }
        }
        if (pass_let) {
            *pass_let = std::move(let);
        }
        else {
            scope->statements.push_back(Stat{let});
        }
        scope->ident_usage.track.push_back({let});
        return true;
    }

    template <class Type>
    bool traverse_return_like(const char* token, const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, token, &pos)) {  // let
            return false;
        }
        skip_space(it, node);
        std::shared_ptr<Expr> val;
        if (auto expr = expect(it, node, "expr")) {
            if (!traverse_expr(scope, val, *expr)) {
                return false;
            }
        }
        scope->statements.push_back(Stat{Type{pos, std::move(val)}});
        return true;
    }

    bool traverse_return(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        return traverse_return_like<Return>("return", scope, node);
    }

    bool traverse_break(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        return traverse_return_like<Break>("break", scope, node);
    }

    bool traverse_continue(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        return traverse_return_like<Continue>("continue", scope, node);
    }

    bool traverse_scope(const std::shared_ptr<Scope>& scope, const json::JSON& node);

    bool traverse_attached_block(const std::shared_ptr<Scope>& new_scope, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        if (!expect_token(it, node, "{", &pos)) {  // {
            return false;
        }
        new_scope->block_begin = pos;
        while (true) {
            skip_space(it, node);
            if (auto tok = expect_token(it, node, "}", &pos)) {
                new_scope->block_end = pos;
                break;
            }
            if (!traverse_scope(new_scope, *it)) {
                return false;
            }
            it++;
        }
        return true;
    }

    bool traverse_label(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        auto it = node.abegin();
        skip_space(it, node);
        Pos pos;
        auto ident = expect_with_pos(it, node, "ident", &pos);
        if (!ident) {
            return false;
        }
        auto lab = std::make_shared<Label>();
        lab->ident = ident->force_as_string<std::string>();
        lab->pos = pos;
        scope->statements.push_back({std::move(lab)});
        return true;
    }

    bool traverse_block(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        auto new_scope = std::make_shared<Scope>();
        new_scope->parent = scope;
        return traverse_attached_block(new_scope, node);
    }

    bool traverse_scope(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        if (auto prg = node.at("program")) {
            for (auto& it : json::as_array(*prg)) {
                if (!traverse_scope(scope, it)) {
                    return false;
                }
            }
        }
        else if (auto fn = node.at("fn_def")) {
            return traverse_fn(scope, *fn);
        }
        else if (auto block = node.at("block")) {
            return traverse_block(scope, *block);
        }
        else if (auto let = node.at("let")) {
            return traverse_let(scope, nullptr, *let);
        }
        else if (auto expr = node.at("expr")) {
            std::shared_ptr<Expr> tmp;
            if (!traverse_expr(scope, tmp, *expr)) {
                return false;
            }
            scope->statements.push_back(Stat{std::move(tmp)});
        }
        else if (auto nst = node.at("token"); nst && nst->force_as_string<std::string>() == ";") {
            scope->statements.push_back(Stat{std::monostate{}});  // null statement
        }
        else if (auto ret = node.at("return")) {
            return traverse_return(scope, *ret);
        }
        else if (auto ret = node.at("break")) {
            return traverse_break(scope, *ret);
        }
        else if (auto ret = node.at("continue")) {
            return traverse_continue(scope, *ret);
        }
        else if (auto lab = node.at("label")) {
            return traverse_label(scope, *lab);
        }
        return true;  // null statement (;) or undefined
    }

    bool traverse(const std::shared_ptr<Scope>& scope, const json::JSON& node) {
        return traverse_scope(scope, node);
    }
}  // namespace combl::trvs
