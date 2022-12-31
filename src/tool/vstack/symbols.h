/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// symbols - symbol
#pragma once
#include <minilang/token/token.h>
#include <vector>
#include <unordered_map>
#include <minilang/token/ident.h>
#include <minilang/token/ctxbase.h>
#include <unordered_set>
#include "log.h"

namespace vstack {
    namespace token = utils::minilang::token;
    namespace symbol {
        enum class State {
            ident,
            type_ident,
            fndef,
            fnargdef,
            vardef,
        };

        struct RefMap;

        struct InverseRef : token::Token {
            InverseRef()
                : Token(token::Kind::ref_) {
                pos = {};
            }
            std::shared_ptr<token::Token> defined_at;

            std::unordered_map<token::Pos, std::shared_ptr<token::Token>> ref_by;

            std::weak_ptr<RefMap> on_;

            std::string token_name;

            std::string qualified_name;

            std::string access_name;
        };

        struct RefMap {
            std::weak_ptr<RefMap> parent;
            std::shared_ptr<token::Token> related_scope;
            std::vector<std::shared_ptr<InverseRef>> vars;

            std::unordered_map<token::Pos, std::shared_ptr<token::Ident>> unresolved;
            std::vector<std::shared_ptr<RefMap>> child;

            std::string scope_prefix;

            bool no_discard = false;

            std::string scope_offset() const {
                return "sizeof(" + scope_name() + ")";
            }

            std::string stack_offset(const std::string& calc_align, const std::string& base = "vstack->hi-vstack->sp") const {
                auto gen = [&](auto&& bs) {
                    return calc_align + "(" + bs + "-" + scope_offset() + ",alignof(" + scope_name() + "))";
                };
                if (auto p = parent.lock()) {
                    return gen(p->stack_offset(calc_align, base));
                }
                return gen(base);
            }

            std::string access_name(const std::string& ident, const std::string& offset) const {
                return "((" + scope_name() + "*)" + offset + ")->" + ident;
            }

            std::string scope_name() const {
                return scope_prefix + "scope";
            }

            std::string scope_struct(const std::string& vstack_type) {
                std::string data;
                data += "typedef struct " + scope_name() + "_tag {\n";
                for (auto it = vars.begin(); it != vars.end(); it++) {
                    // now all varaible are int
                    data += "    int ";
                    data += (*it)->qualified_name;
                    data += ";\n";
                }
                if (related_scope && related_scope->kind == token::Kind::func_) {
                    data += "    int return_value;\n";
                    data += "    void (*return_to)(" + vstack_type + "*);\n";
                }
                data += "} " + scope_name() + ";\n\n";
                return data;
            }

            std::shared_ptr<InverseRef> find_var(const auto& key) {
                for (auto it = vars.rbegin(); it != vars.rend(); it++) {
                    auto& ptr = *it;
                    if (ptr->token_name == key) {
                        return ptr;
                    }
                }
                if (auto p = parent.lock()) {
                    return p->find_var(key);
                }
                return nullptr;
            }

            static std::shared_ptr<InverseRef> define(auto&& refs, const auto& key, std::shared_ptr<token::Token> defined_at) {
                auto ref = std::make_shared<InverseRef>();
                ref->token_name = key;
                ref->defined_at = std::move(defined_at);
                refs.push_back(ref);
                return ref;
            }

            std::shared_ptr<InverseRef> define_var(const auto& key, std::shared_ptr<token::Token> def) {
                return define(vars, key, std::move(def));
            }

            std::shared_ptr<InverseRef> define_func(const auto& key, std::shared_ptr<token::Token> def) {
                return define(vars, key, std::move(def));
            }
        };

        struct Refs {
            std::shared_ptr<RefMap> root;
            std::shared_ptr<RefMap> cur;
            std::unordered_map<std::shared_ptr<token::Token>, std::shared_ptr<RefMap>> scope_inverse;
        };

        constexpr auto is_InverseRef = token::make_is_Token<InverseRef, token::Kind::ref_>();

        template <class... Arg>
        struct Context : token::Context<Arg...> {
            Context(token::Context<Arg...>&& val)
                : token::Context<Arg...>(std::move(val)) {
                refs.root = std::make_shared<RefMap>();
                refs.cur = refs.root;
            }
            bool is_def = false;
            Refs refs;

            std::shared_ptr<InverseRef> lookup_first(const std::shared_ptr<token::Ident>& ident) {
                if (is_def) {
                    return nullptr;
                }
                auto found = refs.cur->find_var(ident->token);
                if (found) {
                    found->ref_by[ident->pos] = ident;
                    refs.cur->unresolved.erase(ident->pos);
                }
                else {
                    refs.cur->unresolved[ident->pos] = ident;
                }
                return found;
            }

            void enter_scope() {
                auto new_map = std::make_shared<RefMap>();
                new_map->parent = refs.cur;
                refs.cur->child.push_back(new_map);
                refs.cur = std::move(new_map);
            }

            void leave_scope(const auto& tok) {
                auto is_need = refs.cur->vars.size() || refs.cur->child.size();
                auto parent = refs.cur->parent.lock();
                if (!is_need && !refs.cur->no_discard) {
                    parent->child.pop_back();
                }
                else {
                    refs.cur->related_scope = tok;
                    refs.scope_inverse[tok] = refs.cur;
                }
                refs.cur = std::move(parent);
            }
        };

        constexpr auto block_scope(auto&& inner) {
            auto enter = [](auto&& src) {
                src.ctx.enter_scope();
                return true;
            };
            auto leave = [](const auto& tok, auto&& src) {
                src.ctx.leave_scope(tok);
                return true;
            };
            return token::ctx_scope(enter, inner, leave);
        }

        auto make_context(auto&&... args) {
            return Context{token::make_context(args...)};
        }

        bool resolve_vars(Log& log, const std::string& parent, size_t index, Refs& ref);

        std::string qualified_ident(const std::shared_ptr<token::Ident>& ident);

        struct VstackFn {
            std::vector<std::shared_ptr<token::Token>> units;
        };

    }  // namespace symbol
}  // namespace vstack
