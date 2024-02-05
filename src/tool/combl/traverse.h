/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <json/json_export.h>
#include <variant>
#include <memory>
namespace combl::trvs {
    namespace json = futils::json;
    struct Scope;

    struct Pos {
        size_t begin = 0;
        size_t end = 0;
    };

    struct Type {
        std::string type;
        std::shared_ptr<Type> base;
    };

    struct Param {
        std::string name;
        std::shared_ptr<Type> type;
    };

    struct FnType : Type {
        std::vector<Param> args;
    };

    enum class ExprType {
        bin,
        integer,
        ident,
        fn,
        call,
        par,
        str,
        index,
        slice,
        for_,
        if_,
    };

    struct Expr {
        const ExprType type;

        virtual Pos expr_range() const {
            return {};
        }

       protected:
        Expr(ExprType ty)
            : type(ty) {}
    };

    struct ParExpr : Expr {
        Pos begin_pos;
        std::shared_ptr<Expr> inner;
        Pos end_pos;
        ParExpr()
            : Expr(ExprType::par) {}
        virtual Pos expr_range() const override {
            return {begin_pos.begin, end_pos.end};
        }
    };

    struct CallExpr : Expr {
        std::shared_ptr<Expr> callee;
        Pos begin_pos;
        std::shared_ptr<Expr> arguments;
        Pos end_pos;
        CallExpr()
            : Expr(ExprType::call) {}

        virtual Pos expr_range() const override {
            return {callee->expr_range().begin, end_pos.end};
        }
    };

    struct IndexExpr : Expr {
        std::shared_ptr<Expr> target;
        Pos begin_pos;
        std::shared_ptr<Expr> index;
        Pos end_pos;

        IndexExpr()
            : Expr(ExprType::index) {}

        virtual Pos expr_range() const override {
            return {target->expr_range().begin, end_pos.end};
        }
    };

    struct SliceExpr : Expr {
        std::shared_ptr<Expr> target;
        Pos begin_pos;
        std::shared_ptr<Expr> begin;
        Pos colon_pos;
        std::shared_ptr<Expr> end;
        Pos end_pos;

        SliceExpr()
            : Expr(ExprType::slice) {}

        virtual Pos expr_range() const override {
            return {target->expr_range().begin, end_pos.end};
        }
    };

    enum class StrType {
        normal,
        char_,
        raw,
    };

    struct String : Expr {
        Pos pos;
        StrType str_type;
        std::string literal;
        String()
            : Expr(ExprType::str) {}
        virtual Pos expr_range() const override {
            return pos;
        }
    };

    struct Integer : Expr {
        Pos pos;
        std::string num;

        Integer()
            : Expr(ExprType::integer) {}
        virtual Pos expr_range() const override {
            return pos;
        }
    };

    struct Ident : Expr {
        Pos pos;
        std::string ident;

        Ident()
            : Expr(ExprType::ident) {}
        virtual Pos expr_range() const override {
            return pos;
        }
    };

    struct Binary : Expr {
        Pos op_pos;
        std::string op;
        std::shared_ptr<Expr> left;
        std::shared_ptr<Expr> right;
        Binary()
            : Expr(ExprType::bin) {}
        virtual Pos expr_range() const override {
            return {left->expr_range().begin, right->expr_range().end};
        }
    };

    struct Let {
        Pos let_pos;
        std::string ident;
        std::shared_ptr<Type> type;
        std::shared_ptr<Expr> init;
    };

    struct If : Expr {
        If()
            : Expr(ExprType::if_) {}
        Pos if_pos;
        std::variant<std::monostate, std::shared_ptr<Expr>, std::shared_ptr<Let>> first;
        Pos delim_pos;
        std::shared_ptr<Expr> second;
        std::shared_ptr<Scope> block;
        Pos else_pos;
        std::variant<std::monostate, std::shared_ptr<Scope>, std::shared_ptr<If>> els;
    };

    struct For : Expr {
        For()
            : Expr(ExprType::for_) {}
        Pos for_pos;
        std::variant<std::monostate, std::shared_ptr<Expr>, std::shared_ptr<Let>> first;
        Pos first_delim_pos;
        std::shared_ptr<Expr> second;
        Pos second_delim_pos;
        std::shared_ptr<Expr> third;
        std::shared_ptr<Scope> block;
    };

    struct Return {
        Pos return_pos;
        std::shared_ptr<Expr> value;
    };

    struct Break {
        Pos break_pos;
        std::shared_ptr<Expr> value;
    };

    struct Continue {
        Pos continue_pos;
        std::shared_ptr<Expr> value;
    };

    struct Label {
        Pos pos;
        std::string ident;
    };

    struct Func;

    using IdentEvent = std::variant<std::shared_ptr<Scope>, std::shared_ptr<Let>, std::shared_ptr<Func>, std::shared_ptr<Ident>>;

    struct IdentUsage {
        std::vector<IdentEvent> track;
    };

    struct FuncAttr {
        std::weak_ptr<Scope> parent;
        Pos fn_pos;
        std::vector<Param> args;
        std::shared_ptr<Type> rettype;
        std::shared_ptr<Scope> block;
    };

    struct Func {
        std::string name;
        FuncAttr attr;
    };

    struct Stat {
        std::variant<std::monostate, std::shared_ptr<Expr>,
                     std::shared_ptr<Func>, std::shared_ptr<Scope>,
                     std::shared_ptr<Let>, Return, Break, Continue,
                     std::shared_ptr<Label>>
            stat;
    };

    struct Scope {
        std::weak_ptr<Scope> parent;
        Pos block_begin;
        std::vector<Stat> statements;
        Pos block_end;

        IdentUsage ident_usage;
    };

    struct FuncExpr : Expr {
        FuncAttr attr;
        FuncExpr()
            : Expr(ExprType::fn) {}

        virtual Pos expr_range() const override {
            return {attr.fn_pos.begin, attr.block->block_end.end};
        }
    };

    bool traverse(const std::shared_ptr<Scope>& scope, const json::JSON& node);

    struct Error {
        std::string reason;
        Pos pos;
    };

    void collect_closure_var_candidate(const std::shared_ptr<Scope>& s, auto&& set) {
        if (!s) {
            return;
        }
        auto par = s->parent.lock();
        if (!par) {
            return;
        }
        auto& tr = s->ident_usage.track;
        const auto size = tr.size();
        for (size_t i = 0; i < size; i++) {
            if (std::holds_alternative<std::shared_ptr<Scope>>(tr[i])) {
                if (std::get<std::shared_ptr<Scope>>(tr[i]) == s) {
                    break;
                }
            }
            if (std::holds_alternative<std::shared_ptr<Func>>(tr[i])) {
                set(std::get<std::shared_ptr<Func>>(tr[i]));
            }
            else if (std::holds_alternative<std::shared_ptr<Let>>(tr[i])) {
                set(std::get<std::shared_ptr<Let>>(tr[i]));
            }
        }
    }

}  // namespace combl::trvs
