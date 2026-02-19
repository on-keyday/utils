/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "traverse.h"
#include <deque>
#include "istr.h"

namespace combl::ipret {
    using Value = istr::Value;

    struct Stack : std::enable_shared_from_this<Stack> {
        std::weak_ptr<Stack> parent;
        std::shared_ptr<Stack> child;

        // statements
        std::shared_ptr<trvs::Scope> current_scope;
        trvs::FuncAttr* func = nullptr;

        // symbol table
        std::map<std::string, Value> vars;
        std::map<std::string, std::shared_ptr<trvs::FuncAttr>> funcs;

        // evalution stack

        size_t stat_index = 0;
        istr::Eval eval;

        std::variant<std::monostate, std::weak_ptr<Value>, trvs::FuncAttr*> lookup(const std::string& name, bool as_callee) {
            if (as_callee) {
                if (auto found = funcs.find(name); found != funcs.end()) {
                    return found->second.get();
                }
                if (auto found = vars.find(name); found != vars.end()) {
                    return std::shared_ptr<Value>(shared_from_this(), &found->second);
                }
            }
            else {
                if (auto found = vars.find(name); found != vars.end()) {
                    return std::shared_ptr<Value>(shared_from_this(), &found->second);
                }
                if (auto found = funcs.find(name); found != funcs.end()) {
                    return found->second.get();
                }
            }
            if (auto p = parent.lock()) {
                return p->lookup(name, as_callee);
            }
            return std::monostate{};
        }
    };

    struct InEnv {
        std::shared_ptr<trvs::Scope> global_scope;
        std::shared_ptr<Stack> stack_root;
        std::shared_ptr<Stack> call_stack;
        std::vector<trvs::Error> err;
        std::map<std::string, istr::BuiltinFunc> builtin_funcs;
        istr::Value return_value;

        std::variant<std::monostate, std::weak_ptr<Value>, trvs::FuncAttr*, istr::BuiltinFunc*> lookup(const std::string& name, bool as_callee) {
            auto l = call_stack->lookup(name, as_callee);
            if (l.index() != 0) {
                return std::visit(
                    [](auto& a) {
                        return std::variant<std::monostate, std::weak_ptr<Value>, trvs::FuncAttr*, istr::BuiltinFunc*>{a};
                    },
                    l);
            }
            if (auto found = builtin_funcs.find(name); found != builtin_funcs.end()) {
                return &found->second;
            }
            return std::monostate{};
        }
    };

    bool eval(InEnv& env);
}  // namespace combl::ipret
