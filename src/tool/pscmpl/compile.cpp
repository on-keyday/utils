/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "pscmpl.h"
#include <helper/equal.h>

namespace pscmpl {
    namespace hlp = utils::helper;

    bool is(expr::Expr* p, const char* ty) {
        return !p && hlp::equal(p->type(), ty);
    }
    using namespace utils::wrap;

    bool declare_function(expr::Expr* st, CompileContext& ctx, bool decl) {
        ctx.write("\n", "template<class Input,class Output>\nbool ");
        string key;
        st->stringify(key);
        if (decl) {
            if (auto found = ctx.defs.find(key); found != ctx.defs.end()) {
                ctx.pushstack(st, found->second);
                return ctx.error("name `", key, "` is defined multipley");
            }
            ctx.defs.emplace(key, st);
        }
        ctx.write(key, "(Input&& input,Output& output)");
        if (decl) {
            ctx.write(";\n");
        }
        return true;
    }

    bool compile_binary(expr::Expr* bin, CompileContext& ctx, bool not_consume = false) {
        if (is(bin, "binary")) {
            if (!compile_binary(bin->index(0), ctx)) {
                return false;
            }
            ctx.write(" ");
            bin->stringify(ctx.buffer);
            ctx.write(" ");
            return compile_binary(bin->index(1), ctx);
        }
        else if (is(bin, "call")) {
            bin->stringify(ctx.buffer);
            ctx.write("(");
            auto i = 0;
            for (auto p = bin->index(0); p; i++, p = bin->index(i)) {
                if (i != 0) {
                    ctx.write(" , ");
                }
                if (!compile_binary(p, ctx)) {
                    return false;
                }
            }
            ctx.write(")");
        }
        else if (is(bin, "string")) {
            ctx.write("input");
            if (not_consume) {
                ctx.write(".copy()");
            }
            ctx.write(".expect(u8\"");
            string key;
            bin->stringify(key);
            if (!utils::escape::escape_str(key, ctx.buffer)) {
                ctx.pushstack(bin);
                return ctx.error("failed to escape string");
            }
            ctx.write("\")");
        }
        else if (is(bin, "integer")) {
            ctx.write("input");
            if (not_consume) {
                ctx.write(".copy()");
            }
            ctx.write(".read(");
            bin->stringify(ctx.buffer);
            ctx.write(")");
        }
        else if (is(bin, "bool")) {
            bin->stringify(ctx.buffer);
        }
        else {
            return false;
        }
        return true;
    }

    bool compile_consume(expr::Expr* cs, CompileContext& ctx) {
        ctx.write("// consume tokens\n");
    }

    bool compile_function(expr::Expr* st, CompileContext& ctx) {
        declare_function(st, ctx, false);
        ctx.write(" {\n");
        size_t i = 0;
        for (auto p = st->index(0); p; i++, p = st->index(i)) {
            utils::number::Array<10, char, true> val;
            st->stringify(val);
            auto cmd = [](mnemonic::Command v) {
                return mnemonic::mnemonics[int(v)];
            };
            auto command = [&](mnemonic::Command v) {
                return hlp::equal(val, cmd(v));
            };
            if (command(mnemonic::Command::consume)) {
                if (!compile_consume(st, ctx)) {
                    return false;
                }
            }
        }
        ctx.write("}\n");
    }

    bool compile(expr::Expr* expr, CompileContext& ctx) {
        size_t index = 0;
        for (auto p = expr->index(0); p; index++, p = expr->index(index)) {
            if (!declare_function(p, ctx, true)) {
                return false;
            }
        }
        index = 0;
        for (auto p = expr->index(0); p; index++, p = expr->index(index)) {
            if (!compile_function(p, ctx)) {
                return false;
            }
        }
        return true;
    }
}  // namespace pscmpl