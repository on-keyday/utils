/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "pscmpl.h"
#include <helper/equal.h>

namespace pscmpl {

    bool is(expr::Expr* p, const char* ty) {
        return p && hlp::equal(p->type(), ty);
    }
    using namespace utils::wrap;

    bool compile_each_command(expr::Expr* st, CompileContext& ctx);

    bool declare_function(expr::Expr* st, CompileContext& ctx, bool decl) {
        verbose_parse(st);
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

    bool is_range_expr(expr::Expr* bin, CompileContext& ctx, bool& suc, bool not_consume) {
        utils::number::Array<3, char> v;
        bin->stringify(v);
        if (v.size() == 1 && v[0] == '-' &&
            is(bin->index(0), "string") && is(bin->index(1), "string")) {
            bin->index(0)->stringify(v);
            if (v.size() != 2) {
                ctx.pushstack(bin->index(0));
                suc = ctx.error("expect range expression but string has more than one character");
                return true;
            }
            bin->index(1)->stringify(v);
            if (v.size() != 3) {
                ctx.pushstack(bin->index(1));
                suc = ctx.error("expect range expression but string has more than one character");
                return true;
            }
            if (v[1] > v[2]) {
                ctx.pushstack(bin->index(0), bin->index(1));
                suc = ctx.error("expect range expression but invalid range specified");
                return true;
            }
            ctx.write("((input.current()>='");
            if (!utils::escape::escape_str(hlp::CharView<char>{v[1]}, ctx.buffer)) {
                ctx.pushstack(bin->index(0));
                suc = ctx.error("string escape failed");
                return true;
            }
            ctx.write("'&&input.current()<='");
            if (!utils::escape::escape_str(hlp::CharView<char>{v[2]}, ctx.buffer)) {
                ctx.pushstack(bin->index(1));
                suc = ctx.error("string escape failed");
                return true;
            }
            ctx.write("')?input");
            if (not_consume) {
                ctx.write(".copy()");
            }
            ctx.write(".read(1):false)");
            suc = true;
            return true;
        }
        return false;
    }

    bool compile_binary(expr::Expr* bin, CompileContext& ctx, bool not_consume = false) {
        verbose_parse(bin);
        if (is(bin, "binary")) {
            bool err = false;
            if (is_range_expr(bin, ctx, err, not_consume)) {
                return err;
            }
            if (!compile_binary(bin->index(0), ctx, not_consume)) {
                return false;
            }
            ctx.write(" ");
            string key;
            bin->stringify(key);
            if (key == "=") {
                ctx.pushstack(bin);
                return ctx.error("assigment operator on expr");
            }
            ctx.write(key);
            ctx.write(" ");
            return compile_binary(bin->index(1), ctx, not_consume);
        }
        else if (is(bin, "call")) {
            string key;
            bin->stringify(key);
            ctx.write(key);
            ctx.write("(");
            auto i = 0;
            for (auto p = bin->index(0); p; i++, p = bin->index(i)) {
                if (i != 0) {
                    ctx.write(" , ");
                }
                if (!compile_binary(p, ctx, not_consume)) {
                    return false;
                }
            }
            if (i == 0 && ctx.defs.contains(key)) {
                ctx.write("input");
                if (not_consume) {
                    ctx.write(".copy()");
                }
                ctx.write(",output");
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
        else if (is(bin, "variable")) {
            string key;
            bin->stringify(key);
            if (key == "input" || key == "output") {
                ctx.push(bin);
                return ctx.error(key, " is special word so that you can't use this variable");
            }
            ctx.write(key);
        }
        else if (is(bin, "block")) {
            if (not_consume) {
                ctx.write("[input = input.copy(),&output]");
            }
            else {
                ctx.write("[&input,&output]");
            }
            if (!compile_each_command(bin, ctx)) {
                ctx.push(bin);
                return false;
            }
            ctx.write("()");
        }
        else if (is(bin, "brackets")) {
            ctx.write("(");
            if (!compile_binary(bin->index(0), ctx, not_consume)) {
                return false;
            }
            ctx.write(")");
        }
        else {
            ctx.pushstack(bin);
            return ctx.error("unknown object");
        }
        return true;
    }

    bool compile_consume(expr::Expr* cs, CompileContext& ctx) {
        ctx.write("// consume tokens\n");
        ctx.write("if(!(");
        if (!compile_binary(cs->index(0), ctx, false)) {
            ctx.pushstack(cs);
            return false;
        }
        ctx.write(")) {\nreturn false;\n}\n\n");
        return true;
    }

    bool compile_require(expr::Expr* cs, CompileContext& ctx) {
        ctx.write("// ensure requirement\n");
        ctx.write("if(!(");
        if (!compile_binary(cs->index(0), ctx, true)) {
            ctx.pushstack(cs);
            return false;
        }
        ctx.write(")) {\nreturn false;\n}\n\n");
        return true;
    }

    auto make_tmpvar(CompileContext& ctx) {
        string varname = "tmpvar_";
        utils::number::to_string(varname, ctx.varindex);
        varname.push_back('_');
        ctx.append_random(varname);
        ctx.varindex++;
        return varname;
    }

    bool compile_any(expr::Expr* an, CompileContext& ctx) {
        ctx.write("// consume tokens while condition is true\n");
        auto tmpvar = make_tmpvar(ctx);
        ctx.write("size_t ", tmpvar, " = input.pos();\n");
        ctx.write("while(");
        if (!compile_binary(an->index(0), ctx, false)) {
            ctx.push(an);
            return false;
        }
        ctx.write(") {\n", tmpvar, "= input.pos(); // update index\n}\n");
        ctx.write("input.set_pos(", tmpvar, ");\n");
        return true;
    }

    bool compile_each_command(expr::Expr* st, CompileContext& ctx) {
        parse_msg("command...");
        ctx.write(" {\n\n");
        size_t i = 0;
        for (auto p = st->index(0); p; i++, p = st->index(i)) {
            verbose_parse(p);
            utils::number::Array<10, char, true> val;
            p->stringify(val);
            auto cmd = [](mnemonic::Command v) {
                return mnemonic::mnemonics[int(v)].str;
            };
            auto command = [&](mnemonic::Command v) {
                return hlp::equal(val, cmd(v));
            };
            if (command(mnemonic::Command::consume)) {
                if (!compile_consume(p, ctx)) {
                    ctx.push(st);
                    return false;
                }
            }
            else if (command(mnemonic::Command::require)) {
                if (!compile_require(p, ctx)) {
                    ctx.push(st);
                    return false;
                }
            }
            else if (command(mnemonic::Command::any)) {
                if (!compile_any(p, ctx)) {
                    ctx.push(st);
                    return false;
                }
            }
        }
        ctx.write("return true;\n}\n");
        return true;
    }

    bool compile_function(expr::Expr* st, CompileContext& ctx) {
        declare_function(st, ctx, false);
        return compile_each_command(st, ctx);
    }

    bool compile(expr::Expr* expr, CompileContext& ctx) {
        ctx.write("// generated by pscmpl (https://github.com/on-keyday/utils)\n");
        size_t index = 0;
        parse_msg("declaration...");
        for (auto p = expr->index(0); p; index++, p = expr->index(index)) {
            if (is(p, "struct")) {
                if (!declare_function(p, ctx, true)) {
                    return false;
                }
            }
            else {
                ctx.push(p);
                return ctx.error("unknown object");
            }
        }
        parse_msg("definition...");
        index = 0;
        for (auto p = expr->index(0); p; index++, p = expr->index(index)) {
            if (!compile_function(p, ctx)) {
                return false;
            }
        }

        if (any(ctx.flag & CompileFlag::with_main)) {
            auto found = ctx.defs.find("EntryPoint");
            if (found == ctx.defs.end()) {
                return ctx.error("EntryPoint was not found when --main flag set");
            }
            ctx.write(R"(
#include<file/file_view.h>
#include<core/sequencer.h>

template<class T>
struct Input{
    utils::Sequencer<utils::buffer_t<T>> seq;
    Input(T& t)
        :seq(utils::make_ref_seq(t)){}
    
    Input(T& t,size_t pos)
        :seq(utils::make_ref_seq(t)){
        seq.rptr=pos;
    }
    
    size_t pos() const{
        return seq.rptr;
    }

    void set_pos(size_t pos) {
        seq.rptr=pos;
    }

    Input copy(){
        return Input{seq.buf.buffer,seq.rptr};
    }

    bool expect(auto&& value){
        return seq.seek_if(value);
    }

    auto current() {
        return seq.current();
    }
};

struct Output{};

int main(int argc,char** argv){
    utils::file::View view;
    if(!view.open(argv[1])){
        return -1;
    }
    Output v;
    auto input=Input{view};
    return EntryPoint(input,v)?0:1;
}

)");
        }
        return true;
    }
}  // namespace pscmpl
