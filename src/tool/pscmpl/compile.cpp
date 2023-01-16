/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "pscmpl.h"
#include <helper/equal.h>
#include <view/char.h>

namespace pscmpl {
    namespace view = utils::view;

    using namespace utils::wrap;

    bool compile_each_command(expr::Expr* st, CompileContext& ctx, bool no_return = false);

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
        utils::number::Array<char, 3> v;
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
            if (!utils::escape::escape_str(view::CharView<char>{v[1]}, ctx.buffer)) {
                ctx.pushstack(bin->index(0));
                suc = ctx.error("string escape failed");
                return true;
            }
            ctx.write("'&&input.current()<='");
            if (!utils::escape::escape_str(view::CharView<char>{v[2]}, ctx.buffer)) {
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

    bool is_valid_string_expr(expr::Expr* bin, bool rec = false) {
        if (is(bin, "string")) {
            return true;
        }
        else if (is(bin, "binary")) {
            auto e = static_cast<expr::BinExpr*>(bin);
            if (e->op == expr::Op::add) {
                return is_valid_string_expr(e->left, true) &&
                       is_valid_string_expr(e->right, true);
            }
            return rec;
        }
        return false;
    }

    bool is_valid_arithmetic_binary(expr::Expr* bin, CompileContext& ctx, bool strchecked = false) {
        assert(bin);
        auto expr = static_cast<expr::BinExpr*>(bin);
        auto left = expr->left;
        auto right = expr->right;
        auto op = expr->op;
        if (!expr::is_arithmetic(op)) {
            ctx.push(bin);
            return ctx.error("expect artithmetic operator but not");
        }
        bool muststr = false;
        auto check_vaild = [&](expr::Expr* v) {
            if (is(v, "binary")) {
                return is_valid_arithmetic_binary(v, ctx, strchecked);
            }
            if (is(v, "brackets")) {
                return is_valid_arithmetic_binary(v->index(0), ctx, strchecked);
            }
            if (!is(v, "integer") && !is(v, "variable")) {
                ctx.push(v);
                return ctx.error("boolean value is detected at arithmetic operation");
            }
            return true;
        };
        if (!check_vaild(left) || !check_vaild(right)) {
            return false;
        }
        return true;
    }

    bool is_valid_variable(expr::Expr* bin, string& key, CompileContext& ctx) {
        auto varerr = [&] {
            ctx.push(bin);
            return ctx.error(key, " is invalid c++ variable name");
        };
        if (utils::number::is_digit(key[0])) {
            return varerr();
        }
        bool must = false;
        bool alnum = false;
        for (auto& c : key) {
            if (must) {
                if (c != ':') {
                    return varerr();
                }
                must = false;
                alnum = true;
            }
            else if (c == ':') {
                if (alnum) {
                    return varerr();
                }
                must = true;
            }
            else {
                alnum = false;
            }
        }
        if (must || alnum) {
            return varerr();
        }
        return true;
    }

    bool compile_binary(expr::Expr* bin, CompileContext& ctx, bool not_consume = false, bool arithmetric = false) {
        verbose_parse(bin);
        if (is(bin, "binary")) {
            bool err = false;
            if (is_range_expr(bin, ctx, err, not_consume)) {
                return err;
            }
            auto e = static_cast<expr::BinExpr*>(bin);
            auto& arithmetic_set = err;
            if (!arithmetric && expr::is_arithmetic(e->op)) {
                bool is_str = false;
                if (is_valid_string_expr(bin)) {
                    is_str = true;
                }
                else if (!is_valid_arithmetic_binary(bin, ctx)) {
                    return false;
                }
                arithmetric = true;
                arithmetic_set = true;
                ctx.write("input");
                if (not_consume) {
                    ctx.write(".copy()");
                }
                if (is_str) {
                    ctx.write(".expect(");
                }
                else {
                    ctx.write(".read(");
                }
            }
            if (!compile_binary(bin->index(0), ctx, not_consume, arithmetric)) {
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
            if (!compile_binary(bin->index(1), ctx, not_consume, arithmetric)) {
                return false;
            }
            if (arithmetic_set) {
                ctx.write(")");
            }
        }
        else if (is(bin, "call")) {
            string key;
            bin->stringify(key);
            if (!is_valid_variable(bin, key, ctx)) {
                return false;
            }
            ctx.write(key);
            if (key == "output") {
                auto p = bin->index(0);
                auto ig = bin->index(1);
                if (!p || ig) {
                    ctx.push(bin);
                    return ctx.error("`output` requires one argument but ", ig ? "too many" : "no", " arguments are provided");
                }
                if (!is(p, "string")) {
                    ctx.push(p);
                    return ctx.error("requires string but not");
                }
                p->stringify(ctx.buffer);
            }
            else {
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
        }
        else if (is(bin, "string")) {
            if (!arithmetric) {
                ctx.write("input");
                if (not_consume) {
                    ctx.write(".copy()");
                }
                ctx.write(".expect(");
            }
            ctx.write("u8\"");
            string key;
            bin->stringify(key);
            if (!utils::escape::escape_str(key, ctx.buffer)) {
                ctx.pushstack(bin);
                return ctx.error("failed to escape string");
            }
            ctx.write("\"");
            if (!arithmetric) {
                ctx.write(")");
            }
        }
        else if (is(bin, "integer")) {
            if (!arithmetric) {
                ctx.write("input");
                if (not_consume) {
                    ctx.write(".copy()");
                }
                ctx.write(".read(");
            }
            auto iexpr = static_cast<expr::IntExpr*>(bin);
            iexpr->stringify(ctx.buffer, 10);
            if (!arithmetric) {
                ctx.write(")");
            }
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
            if (!is_valid_variable(bin, key, ctx)) {
                return false;
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
            if (!compile_binary(bin->index(0), ctx, not_consume, arithmetric)) {
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
        ctx.write("input.set_pos(", tmpvar, ");\n\n");
        return true;
    }

    bool compile_vardef(expr::Expr* b, CompileContext& ctx) {
        ctx.write("//define variable\n");
        ctx.write("auto ");
        b->stringify(ctx.buffer);
        ctx.write(" = ");
        if (!compile_binary(b->index(0), ctx, true, true)) {
            ctx.push(b);
            return false;
        }
        ctx.write(";\n\n");
        return true;
    }

    bool compile_bind(expr::Expr* b, CompileContext& ctx, bool bany = false) {
        ctx.write("// bind to variable\n");
        auto bindto = b->index(0);
        auto expr = b->index(1);
        verbose_parse(bindto);
        string name;
        auto tmpvar = make_tmpvar(ctx);
        ctx.write("auto ", tmpvar, " = input.pos();\n");
        ctx.write("if(!(");
        if (!compile_binary(expr, ctx)) {
            ctx.push(b);
            return false;
        }
        if (!bany) {
            ctx.write(")){\nreturn false;\n}\n");
        }
        else {
            ctx.write(")){\ninput.set_pos(", tmpvar, ");\n}\n");
        }
        if (!is(bindto, "string") && !is(bindto, "variable")) {
            ctx.pushstack(bindto, b);
            return ctx.error("expect string or variable for bind but not");
        }
        bindto->stringify(name);
        ctx.write("if(!input.bind(", tmpvar, " , ", is(bindto, "string") ? "output" : "", name, ")){\n");
        ctx.write("return false;\n");
        ctx.write("}\n\n");
        return true;
    }

    bool compile_if(expr::Expr* b, CompileContext& ctx) {
        ctx.write("// if statement\n");
        ctx.write("if(");
        auto cond = b->index(0);
        auto block = b->index(1);
        if (!compile_binary(cond, ctx, true, true)) {
            ctx.push(b);
            return false;
        }
        ctx.write(") ");
        if (!is(block, "block")) {
            ctx.push(block);
            return ctx.error("block expression is expected but not.");
        }
        if (!compile_each_command(block, ctx, true)) {
            ctx.push(block);
            return false;
        }
        return true;
    }

    bool compile_each_command(expr::Expr* st, CompileContext& ctx, bool no_return) {
        parse_msg("command...");
        ctx.write(" {\n\n");
        size_t i = 0;
        for (auto p = st->index(0); p; i++, p = st->index(i)) {
            verbose_parse(p);
            if (is(p, "vardef")) {
                if (!compile_vardef(p, ctx)) {
                    return false;
                }
            }
            else {
                utils::number::Array< char,10, true> val;
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
                else if (command(mnemonic::Command::bind)) {
                    if (!compile_bind(p, ctx)) {
                        ctx.push(st);
                        return false;
                    }
                }
                else if (command(mnemonic::Command::bindany)) {
                    if (!compile_bind(p, ctx, true)) {
                        ctx.push(st);
                        return false;
                    }
                }
                else if (command(mnemonic::Command::if_)) {
                    if (!compile_if(p, ctx)) {
                        ctx.push(st);
                        return false;
                    }
                }
            }
        }
        if (!no_return) {
            ctx.write("return true;");
        }
        ctx.write("\n}\n");
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
            else if (is(p, "import") || is(p, "sysimport")) {
                auto str = p->index(0);
                string key, value;
                str->stringify(key);
                ctx.write("#include ");
                if (!utils::escape::escape_str(key, value)) {
                    ctx.push(str);
                    return ctx.error("failed to escape string");
                }
                if (is(p, "sysimport")) {
                    ctx.write("<", value, ">\n");
                }
                else {
                    ctx.write("\"", value, "\"\n");
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
            if (is(p, "struct")) {
                if (!compile_function(p, ctx)) {
                    return false;
                }
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
    utils::Sequencer<utils::buffer_t<T&>> seq;
    Input(T& t)
        :seq(t){}
    
    Input(T& t,size_t pos)
        :seq(t){
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

    bool read(size_t value){
        return seq.consume(value);
    }

    bool bind(size_t start,auto& tobind){
        return bind_object(*this,start,tobind);
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
