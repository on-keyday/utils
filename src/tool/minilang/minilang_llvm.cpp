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
            else if (seq.seek_if("bool")) {
                if (seq.eos()) {
                    select = "i1";
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
                ctx.error("unsupported type", type);
                return false;
            }
            return true;
        }

        bool dump_expr(Node* node, Context& ctx, LLVMValue& value) {
            auto type = [&](const char* str) {
                return is(node->expr, str);
            };
            auto ch = [&](auto i) {
                return node->child(i);
            };
            if (type("binary")) {
                auto bin = static_cast<expr::BinExpr*>(node->expr);
                if (expr::is_logical(bin->op)) {
                    auto str = bin->op == expr::Op::and_ ? "and." : "or.";
                    if (!dump_expr(ch(0), ctx, value)) {
                        return false;
                    }
                    auto cond = ctx.make_tmp(str, ".begin");
                    ctx.write("br label %", cond, "\n");
                    ctx.write(cond, ":\n");
                    auto then = ctx.make_tmp(str, ".then");
                    auto done = ctx.make_tmp(str, ".done");
                    auto end = ctx.make_tmp(str, ".end");
                    if (bin->op == expr::Op::and_) {
                        ctx.write("br i1 ", value.val, ", label %", then, ", label %", end, "\n");
                    }
                    else {
                        ctx.write("br i1 ", value.val, ", label %", end, ", label %", then, "\n");
                    }
                    ctx.write(then, ":\n");
                    if (!dump_expr(ch(0), ctx, value)) {
                        return false;
                    }
                    ctx.write("br label %", done, "\n");
                    ctx.write(done, ":\n");
                    ctx.write("br label %", end, "\n");
                    ctx.write(end, ":\n");
                    auto phi = ctx.make_tmp("tmp", "");
                    ctx.write("%", phi, " = phi i1 [");
                    if (bin->op == expr::Op::and_) {
                        ctx.write("false");
                    }
                    else {
                        ctx.write("true");
                    }
                    ctx.write(", %", cond, "], [", value.val, ", %", done, "]\n");
                    value.val = "%" + phi;
                    value.node = node;
                    return true;
                }
                else {
                    ctx.error("unimplemented", node);
                    return false;
                }
            }
            else if (type(expr::typeInt)) {
                auto n = static_cast<expr::IntExpr*>(node->expr);
                value.val = "";
                n->stringify(value.val, 10);
                value.type.as_int(32);
                return true;
            }
            else {
                ctx.error("unimplemented", node);
                return false;
            }
            return true;
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
                    ctx.error("now, return type inference is not supported", node);
                    return false;
                }
                ctx.write(" ");
                if (ctx.loc == Location::global) {
                    ctx.write("@");
                }
                else {
                    ctx.write("%");
                }
                ctx.write(fsig->name);
                ctx.write("() ");
                ctx.write("{\nentry:\n");
                auto save = ctx.tmpindex;
                auto locsave = ctx.loc;
                ctx.tmpindex = 0;
                ctx.loc = Location::func;
                if (!dump_llvm(ch(1), ctx)) {
                    return false;
                }
                ctx.loc = locsave;
                ctx.tmpindex = save;
                ctx.write("}\n");
            }
            else if (type("expr_stat")) {
                LLVMValue value;
                if (!dump_expr(node->child(0), ctx, value)) {
                    return false;
                }
            }
            else if (type("return")) {
                LLVMValue value;
                if (!dump_expr(node->child(0), ctx, value)) {
                    return false;
                }
                ctx.write("ret i32 ", value.val, "\n");
            }
            else if (type("program") || type("block")) {
                for (auto i = 0; ch(i); i++) {
                    if (!dump_llvm(ch(i), ctx)) {
                        return false;
                    }
                }
            }
            else {
                ctx.error("unimplemented", node);
                return false;
            }
            return true;
        }
    }  // namespace assembly
}  // namespace minilang
