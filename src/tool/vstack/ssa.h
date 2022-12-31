/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include <minilang/ssa/ssa.h>
#include <minilang/token/tokendef.h>
#include <minilang/token/cast.h>
#include "func.h"

namespace vstack {
    namespace token = utils::minilang::token;
    namespace ssa = utils::minilang::ssa;
    namespace middle {
        constexpr auto k_ident = ssa::make_user_defined_kind(1);
        constexpr auto k_number = ssa::make_user_defined_kind(2);
        constexpr auto k_bin = ssa::make_user_defined_kind(3);
        constexpr auto k_ret = ssa::make_user_defined_kind(4);
        constexpr auto k_call = ssa::make_user_defined_kind(5);
        constexpr auto k_func = ssa::make_user_defined_kind(6);
        constexpr auto k_let = ssa::make_user_defined_kind(7);
        constexpr auto k_block = ssa::make_user_defined_kind(8);

        struct Number : ssa::Primitive {
            Number()
                : ssa::Primitive(k_number) {}
            std::shared_ptr<token::Number> token;
        };

        struct Ident : ssa::Primitive {
            Ident()
                : ssa::Primitive(k_ident) {}
            std::shared_ptr<token::Ident> token;
        };

        struct BinOp : ssa::BinOp {
            BinOp()
                : ssa::BinOp(k_bin) {}
            std::shared_ptr<token::BinTree> token;
            std::shared_ptr<token::Symbol<>> op;
        };

        struct Ret : ssa::UnaryOp {
            Ret()
                : ssa::UnaryOp(k_ret) {}
            std::shared_ptr<token::UnaryTree> token;
        };

        struct Call : ssa::Istr {
            Call()
                : ssa::Istr(k_call) {}
            std::shared_ptr<token::Brackets> token;
            std::shared_ptr<ssa::Value> callee;
            std::vector<std::shared_ptr<ssa::Value>> args;
        };

        struct Block : ssa::Pseudo {
            Block()
                : ssa::Pseudo(k_block) {}
            std::shared_ptr<token::Block<>> token;
            ssa::SSA<> ssa;
        };

        struct Func : ssa::Pseudo {
            Func()
                : ssa::Pseudo(k_func) {}
            std::shared_ptr<vstack::Func> token;
            std::shared_ptr<Block> block;
        };

        struct Alloc : ssa::Pseudo {
            Alloc()
                : ssa::Pseudo(k_let) {}
            std::shared_ptr<vstack::Let> token;
            std::vector<std::shared_ptr<ssa::Value>> inits;
        };

        void global_to_ssa(ssa::SSA<>& ssa, const std::shared_ptr<token::Token>& gl);

        constexpr auto to_Func = ssa::make_to_Istr<Func, k_func>();

    }  // namespace middle
}  // namespace vstack
