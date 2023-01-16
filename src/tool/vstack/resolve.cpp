/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "vstack.h"
#include "symbols.h"
#include <number/to_string.h>

namespace vstack {
    namespace symbol {

        bool resolve_vars(Log& log, const std::string& parent, size_t index, Refs& ref) {
            std::string scope = parent;
            if (ref.cur->related_scope) {
                auto kind = ref.cur->related_scope->kind;
                if (kind == token::Kind::block) {
                    scope += "block";
                }
                else if (kind == token::Kind::func_) {
                    scope += "func";
                }
                scope += utils::number::to_string<std::string>(index);
                if (auto fn = is_Func(ref.cur->related_scope)) {
                    scope += "_" + fn->ident->token;
                }
                scope += "_";
            }
            ref.cur->scope_prefix = scope;
            auto offset = ref.cur->stack_offset("calc_align");
            for (auto it = ref.cur->vars.rbegin(); it != ref.cur->vars.rend(); it++) {
                auto& ident = *it;
                auto base = scope;
                auto is_fn = is_Func(ident->defined_at);
                if (is_fn) {
                    base += "func" + utils::number::to_string<std::string>(index) + "_";
                }
                base += ident->token_name;
                auto hash = std::hash<std::string>()(base);
                ident->qualified_name = base + "_" + utils::number::to_string<std::string>(hash, 16);
                ident->on_ = ref.cur;
                if (!is_fn) {
                    ident->access_name = ref.cur->access_name(ident->qualified_name, offset);
                }
            }
            for (auto& unresolv : ref.cur->unresolved) {
                auto looked_up = ref.root->find_var(unresolv.second->token);
                if (!looked_up) {
                    log.error("unresolved identifier");
                    log.token(unresolv.second);
                    return false;
                }
                unresolv.second->second_lookup = looked_up;
            }
            size_t i = 0;
            for (auto& child : ref.cur->child) {
                ref.cur = child;
                if (!resolve_vars(log, scope, i, ref)) {
                    return false;
                }
                i++;
            }
            return true;
        }

        std::string qualified_ident(const std::shared_ptr<token::Ident>& ident) {
            if (auto loc = is_InverseRef(ident->second_lookup.lock())) {
                return loc->qualified_name;
            }
            if (auto loc = is_InverseRef(ident->first_lookup.lock())) {
                return loc->qualified_name;
            }
            return "";
        }
    }  // namespace symbol
}  // namespace vstack
