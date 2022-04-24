/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"
namespace minilang {
    namespace assembly {
        const char* select_primitive(auto& v) {
            auto seq = make_ref_seq(v);
            bool notyet = false;
            const char* select = nullptr;
            if (seq.seek_if("int") || seq.seek_if("uint")) {
                if (seq.eos() || seq.seek_if("32")) {
                    select = "i32";
                }
                else if (seq.seek_if("8")) {
                    select = "i8";
                }
                else if (seq.seek_if("16")) {
                    select = "i16";
                }
                else if (seq.seek_if("64")) {
                    select = "i64";
                }
                else {
                    notyet = true;
                }
                if (!seq.eos()) {
                    notyet = true;
                }
            }
            else if (seq.seek_if("float")) {
                if (seq.eos() || seq.seek_if("32")) {
                    select = "float";
                }
                else if (seq.seek_if("16")) {
                    select = "half";
                }
                else if (seq.seek_if("64")) {
                    select = "double";
                }
                else {
                    notyet = true;
                }
                if (!seq.eos()) {
                    notyet = true;
                }
            }
            if (!select || notyet) {
                select = "void";
            }
            return select;
        }

        bool write_return_type(Node* type, Context& ctx) {
            auto tyexpr = static_cast<TypeExpr*>(type->expr);
            if (tyexpr->kind == TypeKind::ptr) {
                ctx.write("ptr");
            }
            else if (tyexpr->kind == TypeKind::array) {
                ctx.write("void");
            }
            else if (tyexpr->kind == TypeKind::primitive) {
                auto& v = tyexpr->val;
                ctx.write(select_primitive(v));
            }
            else {
                ctx.error("unsupported type");
                return false;
            }
        }

        bool dump_binary(Node* node, Context& ctx, LLVMValue& value) {
            auto type = [&](const char* str) {
                return is(node->expr, str);
            };
            auto ch = [&](auto i) {
                return node->child(i);
            };
            if (type("binary")) {
            }
        }

        bool NodeAnalyzer::dump_llvm(Node* node, Context& ctx) {
            auto type = [&](const char* str) {
                return is(node->expr, str);
            };
            auto ch = [&](auto i) {
                return node->child(i);
            };
            if (type("func")) {
                auto signature = ch(0);
                auto fsig = static_cast<expr::CallExpr<wrap::string, wrap::vector>*>(signature->expr);
                ctx.write("define ");
                if (ch(2)) {
                    if (!write_return_type(ch(2), ctx)) {
                        return false;
                    }
                }
                else {
                    ctx.error("now, return type inference is not supported");
                    return false;
                }
                if (ctx.loc == Location::global) {
                    ctx.write("@");
                }
                else {
                    ctx.write("%");
                }
                ctx.write(fsig->name);
                ctx.write("{");
                auto save = ctx.tmpindex;
                auto locsave = ctx.loc;
                ctx.tmpindex = 0;
                ctx.loc = Location::func;
                if (!dump_llvm(node, ctx)) {
                    return false;
                }
                ctx.loc = locsave;
                ctx.tmpindex = save;
                ctx.write("}");
            }
            else if (type("expr_stat")) {
            }
        }
    }  // namespace assembly
}  // namespace minilang
