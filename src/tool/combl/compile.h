/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include "traverse.h"
#include "istr.h"

namespace combl::bin {

    struct LexStack {
        std::weak_ptr<LexStack> parent;
        std::shared_ptr<trvs::Scope> current_scope;
        size_t index = 0;
        std::shared_ptr<LexStack> child;
    };

    struct Immediate {
        std::shared_ptr<trvs::Integer> imm;
    };

    struct BinOp {
        std::shared_ptr<trvs::Binary> binop;
    };

    struct Instruction {
        std::variant<Immediate, BinOp> istr;
    };

    struct Compiler {
        llvm::LLVMContext ctx;
        std::unique_ptr<llvm::Module> mod;
        std::unique_ptr<llvm::IRBuilder<>> builder;
        std::shared_ptr<trvs::Scope> global_scope;
        std::shared_ptr<LexStack> root_stack;
        std::shared_ptr<LexStack> stack;
        istr::Eval eval;
    };

    bool compile(Compiler& cm);
}  // namespace combl::bin
