/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "writer.h"
#include "ssa.h"

namespace vstack {
    namespace token = utils::minilang::token;

    namespace gen {
        using utils::helper::defer;
        /*
        void write_oneof(write::Stacks& w, write::Writer& local, const std::shared_ptr<token::Token>& tok);

        void write_block(write::Stacks& w, write::Writer& local, const std::shared_ptr<token::Block<>>& block) {
            if (!block) {
                local.w.write_raw("{}\n\n");
                return;
            }
            local.w.write_raw("{\n\n");
            local.w.indent(1);
            for (auto& arg : block->elements) {
                write_oneof(w, local, arg);
            }
            local.w.indent(-1);
            local.w.write_line();
            local.w.write("}");
            local.w.write_line();
        }

        void write_func(write::Stacks& w, const std::shared_ptr<Func>& fn) {
            write::FuncStack s(w.current, fn);
            auto refs = w.refs->scope_inverse[fn];

            // write stack struct
            w.global.header.w.write_raw(refs->scope.make_struct(w.ctx.vstack_type));
            size_t i = 0;

            // write forward declaration
            s.write_declare(w.global.header, w.ctx, i);

            write::Writer local;

            s.write_define(local, w.ctx, i);

            w.global.src.merge_from(local);
        }

        void write_binary(write::Stacks& w, write::Writer& local, const std::shared_ptr<token::Token>& tok) {
            if (auto bin = token::is_BinTree(tok)) {
                auto sym = token::is_Symbol(bin->symbol);
                write_binary(w, local, bin->left);
                if (sym->symbol_kind != s::comma) {
                    local.w.write_raw(" ");
                }
                local.w.write_raw(sym->token, " ");
                write_binary(w, local, bin->right);
                return;
            }
            if (auto br = token::is_Brackets(tok)) {
                auto left = token::is_Symbol(br->left);
                auto right = token::is_Symbol(br->right);
                if (br->callee) {
                    write_binary(w, local, br->callee);
                    if (left->symbol_kind == s::parentheses_begin) {
                        local.w.write_raw("(vstack, ");
                    }
                    else {
                        local.w.write_raw(left->token);
                    }
                }
                else {
                    local.w.write_raw(left->token);
                }
                write_binary(w, local, br->center);
                local.w.write_raw(right->token);
                return;
            }
            if (auto ident = token::is_Ident(tok)) {
                local.w.write_raw(symbol::qualified_ident(ident));
                return;
            }
            if (auto num = token::is_Number(tok)) {
                local.w.write_raw(num->token);
                return;
            }
        }

        void write_let(write::Stacks& w, write::Writer& local, const std::shared_ptr<Let>& tok) {
            std::vector<std::shared_ptr<token::Token>> inits;
            token::tree_to_vec(inits, tok->init, token::symbol_of(token::SymbolKind::comma));
            auto ic = tok->idents.size();
            auto nc = inits.size();
            auto mi = ic < nc ? ic : nc;
            auto mx = ic > nc ? ic : nc;
            for (auto i = 0; i < mi; i++) {
                local.w.write_indent();
                local.w.write_raw("int ", symbol::qualified_ident(tok->idents[i]), " = ");
                write_binary(w, local, inits[i]);
                local.w.write_raw(";\n");
            }
            for (auto i = mi; i < mx; i++) {
                local.w.write_indent();
                local.w.write_raw("int ", symbol::qualified_ident(tok->idents[i]), " = 0");
                local.w.write_raw(";\n");
            }
        }

        void write_oneof(write::Stacks& w, write::Writer& local, const std::shared_ptr<token::Token>& tok) {
            if (auto fn = is_Func(tok)) {
                write_func(w, fn);
            }
            if (token::is_Brackets(tok) || token::is_Ident(tok) || token::is_BinTree(tok) || token::is_Number(tok)) {
                // now always return
                write_binary(w, local, tok);
            }
            if (auto block = token::is_Block(tok)) {
                if (block->begin->kind == token::Kind::bof) {
                    for (auto& arg : block->elements) {
                        write_oneof(w, local, arg);
                    }
                }
                else {
                    local.w.write_indent();
                    write_block(w, local, block);
                }
            }
            if (auto unary = token::is_UnaryTree(tok)) {
                if (auto ret = token::is_KeywordKindof(unary->symbol, k::return_)) {
                    local.w.write_indent();
                    local.w.write_raw("return ");
                    write_oneof(w, local, unary->target);
                    local.w.write_raw(";\n");
                }
                else {
                    write_binary(w, local, unary);
                }
            }
            if (auto let = is_Let(tok)) {
                write_let(w, local, let);
            }
        }

        void write_global(write::Stacks& stack, const std::shared_ptr<token::Token>& fn) {
            stack.current = nullptr;
            write::StackTree root("vstack_", stack.current);
            auto& typ = stack.ctx.vstack_type;
            // clang-format off
            stack.global.header.w.write_raw(R"(// Code generated by vstack
#pragma once
#include<stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct )",stack.ctx.vstack_type, R"(_tag {
    unsigned char* hi;
    unsigned char* lo;
    size_t sp;
} )",stack.ctx.vstack_type,R"(;

)");
            // clang-format on
            write_oneof(stack, stack.global.src, fn);
            // clang-format off
            stack.global.header.w.write_raw(R"(
#ifdef __cplusplus
}
#endif
)");
            // clang-format on
        }*/

        void write_func_head(write::Writer& l, write::Context& ctx, const std::string& base_name, size_t index) {
            l.w.write_indent();
            l.w.write_raw("void ", base_name, "_", utils::number::to_string<std::string>(index), "(", ctx.vstack_type, "* vstack)");
        }

        void write_func(write::Stacks& s, write::Writer& l, const std::shared_ptr<middle::Func>& fn) {
            auto& ssa = fn->block->ssa;
            auto it = ssa.istrs.begin();
            std::vector<std::list<std::shared_ptr<ssa::Value>>> fns;
            fns.push_back({});
            for (; it != ssa.istrs.end(); it++) {
                fns.back().push_back(*it);
                if ((*it)->istr->kind == middle::k_call) {
                    fns.push_back({});
                }
            }
            auto fnscope = s.refs->scope_inverse[fn->token];
            s.global.header.w.write_raw(fnscope->scope_struct(s.ctx.vstack_type));
            if (auto found = s.refs->scope_inverse.find(fn->block->token); found != s.refs->scope_inverse.end()) {
                s.global.header.w.write_raw(found->second->scope_struct(s.ctx.vstack_type));
            }
            size_t i = 0;
            for (auto& part : fns) {
                write_func_head(s.global.header, s.ctx, symbol::qualified_ident(fn->token->ident), i);
                s.global.header.w.write_raw(";\n\n");
                i++;
            }
        }

        void write_istr(write::Stacks& s, write::Writer& l, const std::shared_ptr<ssa::Istr>& ist) {
            if (auto fn = middle::to_Func(ist)) {
                write_func(s, l, fn);
            }
        }

        void write_ssa(write::Stacks& s, write::Writer& l, ssa::SSA<>& ssa) {
            for (auto& ist : ssa.istrs) {
                write_istr(s, l, ist->istr);
            }
        }
    }  // namespace gen
}  // namespace vstack
