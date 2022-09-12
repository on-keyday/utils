/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// c_lang - c language object mapping
#pragma once
#include <minilang/minlfunc.h>
#include <minilang/minlsymbols.h>
#include <vector>

namespace minlc {
    namespace mi = utils::minilang;
    struct ErrC;
    enum CStatementKind {
        csk_global,
        csk_func,
        csk_functy,
        csk_if,
        csk_expr,
        csk_block,
        csk_vardef,
        csk_return,
    };

    struct CStatement {
        const int stkind = csk_global;
        constexpr CStatement() = default;
        constexpr CStatement(int kind)
            : stkind(kind) {}
    };

    using Statements = std::vector<std::shared_ptr<CStatement>>;

    struct CBlock : CStatement {
        Statements statements;
        std::shared_ptr<mi::LocalSymbols> symbols;
        void push_back(std::shared_ptr<CStatement> c) {
            statements.push_back(std::move(c));
        }

        constexpr CBlock()
            : CStatement(csk_block) {}
    };

    struct CExpr : CStatement {
        std::string value;
        std::shared_ptr<CExpr> left;
        std::shared_ptr<CExpr> right;
        std::shared_ptr<mi::MinNode> node;
        constexpr CExpr()
            : CStatement(csk_expr) {}
    };

    struct CIfStat : CStatement {
        constexpr CIfStat()
            : CStatement(csk_if) {}
        std::shared_ptr<CExpr> cond;
    };

    struct CFuncArg {
        std::string type_;
        std::string argname;
        std::shared_ptr<mi::MinNode> base;
    };

    using CFuncArgs = std::vector<CFuncArg>;

    struct CVarDef : CStatement {
        std::shared_ptr<mi::MinNode> base;
        std::string ident;
        std::string type_;
        std::shared_ptr<CExpr> init;
        constexpr CVarDef()
            : CStatement(csk_vardef) {}
    };

    struct CReturnStat : CStatement {
        std::shared_ptr<mi::MinNode> base;
        std::shared_ptr<CExpr> expr;
        constexpr CReturnStat()
            : CStatement(csk_return) {}
    };

    // TODO(on-keyday): mangling with scope infomation
    inline std::string mangling_ident(mi::MinLinkage linkage, std::string& src) {
        return src;
    }

    // TODO(on-keyday): mangling with scope infomation
    inline std::string mangling_type(mi::MinLinkage linkage, std::string& src) {
        return src;
    }
    struct C;

    std::string minl_type_to_c_type(mi::MinLinkage linkage, const std::shared_ptr<mi::MinNode>& node, C& c);
    std::string minl_type_to_func_type(mi::MinLinkage linkage, const std::shared_ptr<mi::FuncNode>& node, C& c);

    std::shared_ptr<CBlock> minl_to_c_block(C& c, ErrC& errc, std::shared_ptr<mi::BlockNode>& block);

    struct CType {
        std::string ident;
        std::shared_ptr<CType> alias_of;
        std::shared_ptr<mi::TypeNode> base;
    };

    struct CStructField {
        std::string ident;
        std::shared_ptr<CType> type;
        std::shared_ptr<CExpr> init;
    };

    struct CStruct : CStatement {
        std::string ident;
        std::vector<CStructField> fields;
    };

    struct CFuncTypedef : CStatement {
        constexpr CFuncTypedef()
            : CStatement(csk_functy) {}
        std::shared_ptr<mi::FuncNode> base;
        std::string return_;
        std::string identifier;
        CFuncArgs arguments;
    };

    struct CFunction;
    bool add_func_block(C& c, ErrC& errc, CFunction& arg, mi::MinLinkage linkage);

    struct CFunction : CStatement {
        constexpr CFunction()
            : CStatement(csk_func) {}
        std::shared_ptr<mi::FuncNode> base;
        std::string return_;
        std::string identifier;
        CFuncArgs arguments;
        std::shared_ptr<CBlock> block;
        bool set_base(C& c, ErrC& errc, std::shared_ptr<mi::FuncNode>& node, mi::MinLinkage linkage) {
            base = node;
            if (node->ident) {
                identifier = node->ident->str;
            }
            std::vector<std::shared_ptr<mi::FuncParamNode>> params;
            mi::FuncParamToVec(params, node);
            for (auto& param : params) {
                CFuncArg arg;
                if (param->ident) {
                    arg.argname = mangling_ident(linkage, param->ident->str);
                }
                arg.type_ = minl_type_to_c_type(linkage, param->type, c);
                arg.base = param;
                arguments.push_back(std::move(arg));
            }
            return_ = minl_type_to_c_type(linkage, node->return_, c);
            add_func_block(c, errc, *this, linkage);
            return true;
        }
    };

    struct CSymbols {
    };

    struct C {
        std::shared_ptr<mi::TopLevelSymbols> symbols;
        std::shared_ptr<mi::LocalSymbols> local_top;
        std::shared_ptr<mi::LocalSymbols> local;
        std::vector<std::shared_ptr<mi::LocalSymbols>> locals;
        Statements statements;
        std::map<std::string, std::shared_ptr<CFunction>> funcs;
        std::map<std::string, std::string> tydef_cache;
        struct scope_leaver {
            C& c;
            ~scope_leaver() {
                c.local = c.local->parent.lock();
                if (!c.local) {
                    c.locals.push_back(std::move(c.local_top));
                }
            }
        };

        [[nodiscard]] scope_leaver enter_scope(const std::shared_ptr<mi::MinNode>& node) {
            auto locsym = std::make_shared<mi::LocalSymbols>();
            locsym->scope = node;
            if (!local_top) {
                local_top = locsym;
            }
            if (local) {
                local->nexts.push_back(locsym);
                locsym->func = local->func;
            }
            if (auto fn = mi::is_Func(node)) {
                locsym->func = fn;
            }
            locsym->parent = local;
            local = locsym;
            return scope_leaver{*this};
        }

        std::shared_ptr<CFunction> add_function(ErrC& errc, std::shared_ptr<mi::FuncNode>& node, mi::MinLinkage linkage) {
            if (!node) {
                return nullptr;
            }
            auto func = std::make_shared<CFunction>();
            func->set_base(*this, errc, node, linkage);
            statements.push_back(func);
            funcs[func->identifier] = func;
            return func;
        }

        std::shared_ptr<CFuncTypedef> add_functype(const std::shared_ptr<mi::FuncNode>& node) {
            if (!node) {
                return nullptr;
            }
            auto func = std::make_shared<CFuncTypedef>();
            func->base = node;
            statements.push_back(func);
            return func;
        }
    };

    void write_c(C& c, std::string& buf);
}  // namespace minlc
