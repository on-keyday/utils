/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "compile.h"
#include "traverse.h"
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Function.h>

namespace combl::bin {

    llvm::Type* get_llvm_type(Compiler& cm, std::shared_ptr<trvs::Type>& typ) {
        if (typ->type == "int") {
            return llvm::Type::getInt32Ty(cm.ctx);
        }
        else if (typ->type == "char" || typ->type == "byte") {
            return llvm::Type::getInt8Ty(cm.ctx);
        }
        else if (typ->type == "*") {
            return llvm::PointerType::getUnqual(cm.ctx);
        }
        else if (typ->type == "fn") {
            auto fntyp = std::static_pointer_cast<trvs::FnType>(typ);
            std::vector<llvm::Type*> params;
            size_t count = 0;
            for (auto& p : fntyp->args) {
                count++;
                if (!p.type) {
                    continue;
                }
                auto param = get_llvm_type(cm, p.type);
                for (auto i = 0; i < count; i++) {
                    params.push_back(param);
                }
            }
            llvm::Type* ret_type;
            if (typ->base) {
                ret_type = get_llvm_type(cm, typ->base);
            }
            else {
                ret_type = llvm::Type::getVoidTy(cm.ctx);
            }
            return llvm::FunctionType::get(ret_type, params, false);
        }
        return nullptr;
    }

    bool compile_func(Compiler& cm, std::shared_ptr<trvs::Func>& fn) {
        return false;
    }

    bool compile_current(Compiler& cm) {
        auto& stats = cm.stack->current_scope->statements;
        auto& i = cm.stack->index;
        for (; i < stats.size();) {
            auto& op = stats[i].stat;
            if (std::holds_alternative<std::shared_ptr<trvs::Func>>(op)) {
                if (!compile_func(cm, std::get<std::shared_ptr<trvs::Func>>(op))) {
                    return false;
                }
            }
            else if (std::holds_alternative<std::shared_ptr<trvs::Let>>(op)) {
            }
            else if (std::holds_alternative<std::shared_ptr<trvs::Expr>>(op)) {
            }
            else if (std::holds_alternative<trvs::Return>(op)) {
            }
            else if (op.index() == 0) {
                continue;
            }
            else {
            }
        }
        return false;
    }

    bool compile(Compiler& cm) {
        if (!cm.global_scope) {
            return false;
        }
        cm.mod = std::make_unique<llvm::Module>("combl", cm.ctx);
        cm.builder = std::make_unique<llvm::IRBuilder<>>(cm.ctx);
        cm.stack = std::make_shared<LexStack>();
        cm.stack->current_scope = cm.global_scope;
        cm.root_stack = cm.stack;
        return false;
    }
}  // namespace combl::bin
