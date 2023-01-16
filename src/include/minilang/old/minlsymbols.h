/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlsymbols - minilang symbols
#pragma once
#include "minlutil.h"
// using std::map
#include <map>
// using std::vector
#include <vector>

namespace utils {
    namespace minilang {

        struct SymbolList;

        enum MinLinkage {
            mlink_native,
            mlink_file,
            mlink_mod,
            mlink_C,
            mlink_local,
        };

        template <class Node>
        struct MergeNode {
            std::shared_ptr<Node> node;
            MinLinkage linkage;
        };

        template <class Node>
        struct Record {
            std::map<std::string, std::vector<MergeNode<Node>>> node;

            std::vector<MergeNode<Node>>& operator[](const auto& key) {
                return node[key];
            }

            auto find(auto&& name) const {
                return node.find(name);
            }

            auto begin() {
                return node.begin();
            }
            auto end() {
                return node.end();
            }

            auto begin() const {
                return node.begin();
            }
            auto end() const {
                return node.end();
            }
        };

        struct SymbolList {
            Record<MinNode> variables;
            Record<FuncNode> func_defs;
            Record<FuncNode> func_decs;
            Record<LetNode> constants;
            Record<TypeDefNode> types;
            Record<TypeDefNode> type_alias;
        };

        struct TopLevelSymbols : std::enable_shared_from_this<TopLevelSymbols> {
            SymbolList merged;  // all merged
            SymbolList native;
            SymbolList c;
            SymbolList file;
            SymbolList mod;
        };

        struct LocalSymbols : std::enable_shared_from_this<LocalSymbols> {
            std::weak_ptr<LocalSymbols> parent;
            std::shared_ptr<FuncNode> func;
            std::shared_ptr<MinNode> scope;
            SymbolList list;
            std::vector<std::shared_ptr<LocalSymbols>> nexts;
        };

        enum lookup_mask {
            lm_var = 0x01,
            lm_fndef = 0x02,
            lm_fndec = 0x04,
            lm_fn = lm_fndef | lm_fndec,
            lm_const = 0x08,
            lm_type = 0x10,
            lm_type_alias = 0x20,
            lm_special = 0x40,
            lm_ident = lm_var | lm_fndef | lm_fndec | lm_const,
            lm_types = lm_type | lm_type_alias,
        };

        struct LookUpResult {
            unsigned int depth = 0;
            unsigned int found_mask = 0;
            SymbolList* list = nullptr;
            std::shared_ptr<LocalSymbols> local;
            std::shared_ptr<TopLevelSymbols> toplevel;
        };

        bool search_list(LookUpResult& result, auto&& name, SymbolList& list, unsigned int search_mask) {
            bool ok = false;
            auto found = [&](auto& table) {
                if (table.find(name) != table.end()) {
                    result.list = &list;
                    ok = true;
                    return true;
                }
                return false;
            };
            auto mask_and_set = [&](auto& table, auto mask) {
                if (search_mask & mask) {
                    if (found(table)) {
                        result.found_mask |= mask;
                    }
                }
            };
            mask_and_set(list.variables, lm_var);
            mask_and_set(list.func_defs, lm_fndef);
            mask_and_set(list.func_decs, lm_fndec);
            mask_and_set(list.constants, lm_const);
            mask_and_set(list.types, lm_type);
            mask_and_set(list.type_alias, lm_type_alias);
            return ok;
        }

        void lookup_parent_impl(
            LookUpResult& result,
            auto&& name, const unsigned int search_mask, const std::shared_ptr<LocalSymbols>& current,
            const std::shared_ptr<TopLevelSymbols>& toplevel, unsigned int depth, auto&& callback) {
            auto search = [&](SymbolList& list) {
                return search_list(result, name, list, search_mask);
            };
            if (!current) {
                if (toplevel && search(toplevel->merged)) {
                    result.toplevel = toplevel;
                    result.depth = depth;
                    callback(result);
                }
                return;
            }
            if (search(current->list)) {
                result.local = current;
                result.depth = depth;
                if (!callback(result)) {
                    return;
                }
            }
            result = {};  // reset
            return lookup_parent_impl(result, name, search_mask, current->parent.lock(),
                                      toplevel, depth + 1, callback);
        }

        LookUpResult lookup_a_parent(auto&& name, const unsigned int search_mask, const std::shared_ptr<LocalSymbols>& current,
                                     const std::shared_ptr<TopLevelSymbols>& toplevel, unsigned int count = 0) {
            LookUpResult result;
            unsigned int cur = 0;
            bool found = false;
            lookup_parent_impl(result, name, search_mask, current, toplevel, 0, [&](LookUpResult& result) {
                if (cur == count) {
                    found = true;
                    return false;
                }
                return true;  // continue
            });
            if (found) {
                return {};
            }
            return result;
        }

        void lookup_parents(auto&& vec, auto&& name, const unsigned int search_mask, const std::shared_ptr<LocalSymbols>& current,
                            const std::shared_ptr<TopLevelSymbols>& toplevel) {
            LookUpResult result;
            lookup_parent_impl(result, name, search_mask, current, toplevel, 0, [&](LookUpResult& v) {
                vec.push_back(std::move(v));
                return true;
            });
        }

        [[nodiscard]] inline std::shared_ptr<LocalSymbols> enter_local_scope(std::shared_ptr<LocalSymbols>& current,
                                                                             const std::shared_ptr<MinNode>& node) {
            auto symbols = std::make_shared<LocalSymbols>();
            symbols->parent = current;
            symbols->scope = node;
            if (current) {
                symbols->func = current->func;
                symbols->nexts.push_back(symbols);
            }
            if (auto fn = is_Func(node)) {
                symbols->func = std::move(fn);
            }
            return symbols;
        }

        inline std::shared_ptr<TopLevelSymbols> make_toplevel_symbols() {
            return std::make_shared<TopLevelSymbols>();
        }

        inline bool collect_if(MinLinkage linkage, SymbolList& symbol, SymbolList* merged, const std::shared_ptr<MinNode>& target) {
            if (!target) {
                return false;
            }
            if (auto tdef = is_TypeDef(target)) {
                std::vector<std::shared_ptr<TypeDefNode>> new_type, alias;
                TydefToVec(new_type, alias, tdef);
                for (auto& v : new_type) {
                    symbol.types[v->ident->str].push_back({v, linkage});
                    if (merged) {
                        merged->types[v->ident->str].push_back({v, linkage});
                    }
                }
                for (auto& v : alias) {
                    symbol.type_alias[v->ident->str].push_back({v, linkage});
                    if (merged) {
                        merged->type_alias[v->ident->str].push_back({v, linkage});
                    }
                }
            }
            else if (auto let = is_Let(target)) {
                std::vector<std::shared_ptr<LetNode>> defs;
                LetToVec(defs, let);
                if (let->str == const_def_group_str_) {
                    for (auto& v : defs) {
                        symbol.constants[v->ident->str].push_back({v, linkage});
                        if (merged) {
                            merged->constants[v->ident->str].push_back({v, linkage});
                        }
                    }
                }
                else {
                    for (auto& v : defs) {
                        symbol.variables[v->ident->str].push_back({v, linkage});
                        if (merged) {
                            merged->variables[v->ident->str].push_back({v, linkage});
                        }
                    }
                }
            }
            else if (auto fn = is_Func(target)) {
                if (fn->mode == fe_def) {
                    if (fn->str == func_def_str_) {
                        symbol.func_defs[fn->ident->str].push_back({fn, linkage});
                        if (merged) {
                            merged->func_defs[fn->ident->str].push_back({fn, linkage});
                        }
                    }
                    else {
                        symbol.func_decs[fn->ident->str].push_back({fn, linkage});
                        if (merged) {
                            merged->func_decs[fn->ident->str].push_back({fn, linkage});
                        }
                    }
                }
            }
            else {
                return false;
            }
            return true;
        }

        bool collect_block_node_symbol_list(MinLinkage linkage, SymbolList& symbol, SymbolList* merged, const std::shared_ptr<MinNode>& node, auto& errc) {
            if (!node) {
                return false;
            }
            auto cur = is_Block(node);
            if (!cur) {
                return false;
            }
            while (cur) {
                auto target = cur->element;
                if (target && target->str == linkage_str_) {
                    errc.say("unexpected linkage. expect linkage at top level scope");
                    errc.node(target);
                    return false;
                }
                collect_if(linkage, symbol, merged, target);
                cur = cur->next;
            }
            return true;
        }

        bool collect_toplevel_symbol(std::shared_ptr<TopLevelSymbols>& symbol, const std::shared_ptr<MinNode>& node, auto&& errc) {
            if (!symbol || !node) {
                return false;
            }
            auto cur = is_Block(node);
            if (!cur) {
                return false;
            }
            while (cur) {
                auto target = cur->element;
                if (target && target->str == linkage_str_) {
                    auto bin = is_WordStat(target);
                    bool ok = true;
                    if (bin->expr->str == R"("C")") {
                        ok = collect_block_node_symbol_list(mlink_C, symbol->c, &symbol->merged, bin->block, errc);
                    }
                    else if (bin->expr->str == R"("file")") {
                        ok = collect_block_node_symbol_list(mlink_file, symbol->file, &symbol->merged, bin->block, errc);
                    }
                    else if (bin->expr->str == R"("mod")") {
                        ok = collect_block_node_symbol_list(mlink_mod, symbol->mod, &symbol->merged, bin->block, errc);
                    }
                    else {
                        errc.say("unexpected linkage ", bin->expr->str);
                        errc.node(target);
                        return false;
                    }
                    if (!ok) {
                        return false;
                    }
                }
                else {
                    collect_if(mlink_native, symbol->native, &symbol->merged, target);
                }
                cur = cur->next;
            }
            return true;
        }

        constexpr auto duplicate_func_checker(auto& errc) {
            return [&](auto ident, auto&&... funcs) {
                auto fold = [&](auto&& node) {
                    auto& def = node.func_defs[ident];
                    if (def.size() > 1) {
                        errc.say("duplicate entry of ", ident, " function in same linkage");
                        for (auto& v : def) {
                            errc.say("duplicated ", ident);
                            errc.node(v);
                        }
                        return false;
                    }
                    return true;
                };
                return (... && fold(funcs));
            };
        }

        void print_found(auto&& errc, auto&& view, auto&& results) {
            for (LookUpResult& result : results) {
                if (result.toplevel) {
                    errc.say("at toplevel scope");
                }
                else {
                    if (result.local->func) {
                        errc.say("at function `", result.local->func->ident->str, "` scope");
                        errc.node(result.local->func);
                    }
                    else {
                        errc.say("at block scope");
                        errc.node(result.local->scope);
                    }
                }
                auto print_table = [&](auto name, auto& table, auto mask) {
                    if (result.found_mask & mask) {
                        for (auto& var : table[view]) {
                            errc.say(name, " `", view, "` is here");
                            errc.node(var.node);
                        }
                    }
                };
                print_table("variable", result.list->variables, lm_var);
                print_table("constant value", result.list->constants, lm_const);
                print_table("function defintiion", result.list->func_defs, lm_fndef);
                print_table("function declaration", result.list->func_decs, lm_fndec);
                print_table("type definition", result.list->types, lm_types);
                print_table("type alias definition", result.list->type_alias, lm_type_alias);
            }
        }

    }  // namespace minilang
}  // namespace utils
