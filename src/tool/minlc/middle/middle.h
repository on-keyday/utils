/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// c_lang - c language objects
// (refactor)
#pragma once
#include <minilang/minlsymbols.h>
#include "../errc.h"
#include <vector>
#include <string_view>
#include <unordered_map>
#include "type.h"

namespace minlc {
    namespace mi = utils::minilang;
    template <class T>
    using sptr = std::shared_ptr<T>;
    struct ErrC;
    namespace util {
        std::string gen_random(size_t len);
        sptr<mi::TypeNode> gen_type(std::string_view type);
    }  // namespace util
    // middle is intermediate object between minilang and c lang
    namespace middle {
        enum kind {
            mk_block,
            mk_expr,
            mk_binary,
            mk_call,
            mk_par,  // ()
            mk_ident,
            mk_func,
            mk_return,
            mk_program,
            mk_comment,
            mk_define_expr,
            mk_type_expr,
            mk_func_expr,
            mk_dot_expr,
            mk_if,
            mk_for,
            mk_address,
            mk_deref,
        };

        struct Object {
            const kind obj_type;
            bool const_eval = false;
            constexpr Object(int id)
                : obj_type(kind(id)) {}
        };

        struct Block : Object {
            MINL_Constexpr Block()
                : Object(mk_block) {}
            sptr<mi::BlockNode> node;
            std::vector<sptr<Object>> objects;
        };

        struct FuncParam {
            std::shared_ptr<mi::FuncParamNode> node;
            MINL_Constexpr FuncParam() = default;
            FuncParam(const decltype(node)& n)
                : node(n) {}
        };

        struct Func : Object {
            MINL_Constexpr Func()
                : Object(mk_func) {}
            std::shared_ptr<mi::FuncNode> node;
            std::vector<FuncParam> args;
            sptr<Block> block;
            std::shared_ptr<types::FunctionType> type;
        };

        using LookUpResults = std::vector<mi::LookUpResult>;

        struct Expr : Object {
            MINL_Constexpr Expr()
                : Object(mk_expr) {}
            sptr<mi::MinNode> node;
            sptr<types::Type> type;

           protected:
            MINL_Constexpr Expr(int id)
                : Object(id) {}
        };

        struct IdentExpr : Expr {
            MINL_Constexpr IdentExpr()
                : Expr(mk_ident) {}
            bool callee = false;  // callee is operand of left hand
            LookUpResults lookedup;
            sptr<mi::LocalSymbols> current;
        };

        struct DotExpr : Expr {
            MINL_Constexpr DotExpr()
                : Expr(mk_dot_expr) {}
            bool callee = false;
            LookUpResults lookedup;
            sptr<mi::LocalSymbols> current;
            std::vector<sptr<mi::MinNode>> dots;
        };

        struct BinaryExpr : Expr {
            MINL_Constexpr BinaryExpr()
                : Expr(mk_binary) {}
            sptr<Expr> left;
            sptr<Expr> right;
        };

        struct CallExpr : Expr {
            MINL_Constexpr CallExpr()
                : Expr(mk_call) {}
            sptr<Expr> callee;
            std::vector<sptr<Expr>> arguments;
        };

        struct ParExpr : Expr {
            MINL_Constexpr ParExpr()
                : Expr(mk_par) {}
            sptr<Expr> target;
        };

        struct TypeExpr : Expr {
            MINL_Constexpr TypeExpr()
                : Expr(mk_type_expr) {}
            bool callee = false;  // if callee is true, should be cast
        };

        struct FuncExpr : Expr {
            MINL_Constexpr FuncExpr()
                : Expr(mk_func_expr) {}
            sptr<Func> func;
        };

        struct Return : Object {
            constexpr Return()
                : Object(mk_return) {}
            sptr<mi::WordStatNode> node;
            sptr<Expr> expr;
        };

        struct Comment : Object {
            constexpr Comment()
                : Object(mk_comment) {}
            sptr<mi::CommentNode> node;
        };

        struct DefinePair {
            std::vector<sptr<mi::MinNode>> ident;
            std::vector<sptr<Expr>> expr;
        };

        struct DefineExpr : Object {
            MINL_Constexpr DefineExpr()
                : Object(mk_define_expr) {}
            sptr<mi::BinaryNode> node;
            DefinePair define;
        };

        struct If : Object {
            MINL_Constexpr If()
                : Object(mk_if) {}
            std::shared_ptr<mi::IfStatNode> node;
            std::shared_ptr<DefineExpr> inidef;
            std::shared_ptr<Expr> iniexpr;
            std::shared_ptr<Expr> expr;
            std::shared_ptr<Block> block;
            std::shared_ptr<If> else_if;
            std::shared_ptr<Block> els;
        };

        struct For : Object {
            MINL_Constexpr For()
                : Object(mk_for) {}
            std::shared_ptr<mi::ForStatNode> node;
            std::shared_ptr<DefineExpr> inidef;
            std::shared_ptr<Expr> iniexpr;
            std::shared_ptr<Expr> condexpr;
            std::shared_ptr<Expr> eachloopexpr;
            std::shared_ptr<Block> block;
        };

        struct Program : Object {
            MINL_Constexpr Program()
                : Object(mk_program) {}
            sptr<mi::BlockNode> node;
            std::vector<sptr<Func>> global_funcs;
        };

        struct M {
            sptr<mi::TopLevelSymbols> global;
            std::vector<sptr<mi::LocalSymbols>> locals;
            sptr<mi::LocalSymbols> current;
            std::map<std::string, LookUpResults> ident_lookup_cache;
            std::map<std::string, LookUpResults> type_lookup_cache;
            std::unordered_multimap<sptr<mi::MinNode>, sptr<Object>> object_mapping;
            types::Types types;

            struct leave_scope {
                M& m;
                ~leave_scope() {
                    m.current = m.current->parent.lock();
                    m.ident_lookup_cache.clear();  // clear cache
                    m.type_lookup_cache.clear();
                }
            };

            [[nodiscard]] leave_scope enter_scope(const sptr<mi::MinNode>& node) {
                auto new_scope = mi::enter_local_scope(current, node);
                if (!current) {
                    locals.push_back(new_scope);
                }
                current = new_scope;
                ident_lookup_cache.clear();  // clear cache
                type_lookup_cache.clear();
                return leave_scope{*this};
            }

            void add_function(const sptr<mi::FuncNode>& node) {
                if (!current) {
                    return;  // global scope; already added
                }
                if (!node->ident) {
                    node->ident = mi::make_ident_node("anonymous_fn_local_" + util::gen_random(30), mi::invalid_pos);
                }
                if (node->block) {
                    current->list.func_defs[node->ident->str].push_back({node});
                }
                else {
                    current->list.func_decs[node->ident->str].push_back({node});
                }
                ident_lookup_cache.erase(node->ident->str);  // clear ident cache
            }

            void add_variable(auto&& name, const sptr<mi::MinNode>& node) {
                if (!current) {
                    return;  // global scope; already added
                }
                current->list.variables[name].push_back({node});
                ident_lookup_cache.erase(name);
            }

            // no-cache
            LookUpResults lookup_func(auto&& name) {
                LookUpResults results;
                mi::lookup_parents(results, name, mi::lm_fndef, current, global);
                return results;
            }

            LookUpResults lookup_ident(auto&& name) {
                if (auto found = ident_lookup_cache.find(name); found != ident_lookup_cache.end()) {
                    return found->second;
                }
                LookUpResults results;
                mi::lookup_parents(results, name, mi::lm_ident, current, global);
                ident_lookup_cache[name] = results;  // add to cache
                return results;
            }

            LookUpResults lookup_type(auto&& name) {
                if (auto found = type_lookup_cache.find(name); found != type_lookup_cache.end()) {
                    return found->second;
                }
                LookUpResults results;
                mi::lookup_parents(results, name, mi::lm_types, current, global);
                type_lookup_cache[name] = results;
                return results;
            }
            ErrC errc;
        };

        sptr<Block> minl_to_block(M& m, const sptr<mi::BlockNode>& node);
        sptr<Expr> minl_to_expr(M& m, const sptr<mi::MinNode>& node, bool callee = false);
        sptr<Func> minl_to_func(M& m, const sptr<mi::FuncNode>& node);
        sptr<DefineExpr> minl_to_define_expr(M& m, const sptr<mi::MinNode>& node);
        sptr<Program> minl_to_program(M& m, const sptr<mi::MinNode>& node);

        template <class T, class N>
        auto make_middle(M& m, const std::shared_ptr<N>& node) {
            auto d = std::make_shared<T>();
            d->node = node;
            if (!node) {
                (void)*node.get();
            }
            m.object_mapping.emplace(node, d);
            return d;
        }
    }  // namespace middle
    namespace resolver {
        bool resolve(middle::M& m);
    }
}  // namespace minlc
