/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "ssa.h"

namespace vstack::middle {

    std::shared_ptr<ssa::Value> bin_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& tok);
    void tok_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& tok);

    std::shared_ptr<ssa::Value> call_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Brackets>& tok) {
        auto call = ssa.istr<Call>();
        call->token = tok;
        call->callee = bin_to_ssa(ssa, tok->callee);
        std::vector<std::shared_ptr<token::Token>> args;
        token::tree_to_vec(args, tok->center, token::symbol_of(token::SymbolKind::comma));
        for (auto it = args.rbegin(); it != args.rend(); it++) {
            call->args.push_back(bin_to_ssa(ssa, *it));
        }
        std::reverse(call->args.begin(), call->args.end());
        return ssa.value(call);
    }

    void let_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<Let>& lt) {
        auto aloc = ssa.istr<Alloc>();
        aloc->token = lt;
        std::vector<std::shared_ptr<token::Token>> inits;
        token::tree_to_vec(inits, lt->init, token::symbol_of(token::SymbolKind::comma));
        for (auto& in : inits) {
            aloc->inits.push_back(bin_to_ssa(ssa, in));
        }
        ssa.value(aloc);
    }

    std::shared_ptr<ssa::Value> bin_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& tok) {
        if (auto ident = token::is_Ident(tok)) {
            auto ist = ssa.istr<Ident>();
            ist->token = std::move(ident);
            return ssa.value(ist);
        }
        if (auto num = token::is_Number(tok)) {
            auto ist = ssa.istr<Number>();
            ist->token = std::move(num);
            return ssa.value(ist);
        }
        if (auto bin = token::is_BinTree(tok)) {
            if (token::is_SymbolKindof(bin->symbol, s::comma)) {
                bin_to_ssa(ssa, bin->left);
                return bin_to_ssa(ssa, bin->right);
            }
            auto ist = ssa.istr<BinOp>();
            ist->left = bin_to_ssa(ssa, bin->left);
            ist->right = bin_to_ssa(ssa, bin->right);
            ist->token = std::move(bin);
            ist->op = token::is_Symbol(ist->token->symbol);
            return ssa.value(ist);
        }
        if (auto br = token::is_Brackets(tok)) {
            if (br->callee) {
                return call_to_ssa(ssa, br);
            }
            return bin_to_ssa(ssa, br->center);
        }
        return nullptr;
    }

    std::shared_ptr<Block> block_to_ssa(const std::shared_ptr<token::Block<>>& block) {
        auto ist = std::make_shared<Block>();
        ist->token = block;
        for (auto& elm : block->elements) {
            tok_to_ssa(ist->ssa, elm);
        }
        return ist;
    }

    void global_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& gl) {
        auto glblock = token::is_Block(gl);
        for (auto& elm : glblock->elements) {
            tok_to_ssa(ssa, elm);
        }
    }

    void tok_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& tok) {
        if (auto fn = is_Func(tok)) {
            auto ist = ssa.istr<Func>();
            ist->token = std::move(fn);
            ist->block = block_to_ssa(ist->token->block);
            ssa.value(ist);
        }
        if (auto lt = is_Let(tok)) {
            let_to_ssa(ssa, lt);
        }
        if (auto bl = token::is_Block(tok)) {
            global_to_ssa(ssa, bl);
        }
        if (token::is_Ident(tok) || token::is_Number(tok) ||
            token::is_BinTree(tok) || token::is_Brackets(tok)) {
            bin_to_ssa(ssa, tok);
        }
        if (auto ur = token::is_UnaryTree(tok)) {
            if (token::is_KeywordKindof(ur->symbol, k::return_)) {
                auto ist = ssa.istr<Ret>();
                ist->token = std::move(ur);
                ist->target = bin_to_ssa(ssa, ist->token->target);
                ssa.value(ist);
            }
        }
        // unhandled
    }

}  // namespace vstack::middle
