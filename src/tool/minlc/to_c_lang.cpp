/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "c_lang.h"
#include <helper/indent.h>

namespace minlc {
    using Wr = utils::helper::IndentWriter<std::string&>;
    struct write_start {
        Wr& w;
        bool sc = false;
        write_start(Wr& w, bool need_sc = true)
            : w(w), sc(need_sc) {
            w.write_indent();
        }

        ~write_start() {
            if (sc) {
                w.write_raw(";");
            }
            w.write_line();
        }
    };

    void write_statements(Wr& w, Statements& statements);

    void write_expr(Wr& w, std::shared_ptr<CExpr>& expr) {
        const auto need_bracket = expr->left || expr->right;
        if (expr->value == "()") {
            if (expr->left) {
                write_expr(w, expr->left);
            }
            w.write_raw("(");
            if (expr->right) {
                write_expr(w, expr->right);
            }
            w.write_raw(")");
            return;
        }
        if (expr->left) {
            write_expr(w, expr->left);
        }
        w.write_raw(expr->value);
        if (expr->right) {
            write_expr(w, expr->right);
        }
    }

    void write_return(Wr& w, std::shared_ptr<CReturnStat>& stat) {
        write_start s{w};
        w.write_raw("return");
        if (stat->expr) {
            w.write_raw(" ");
            write_expr(w, stat->expr);
        }
    }

    void write_block(Wr& w, std::shared_ptr<CBlock>& block, bool need_start) {
        if (need_start) {
            w.write("{");
        }
        w.indent(+1);
        write_statements(w, block->statements);
        w.indent(-1);
        w.write_indent();
        w.write_raw("}");
    }

    void write_arguments(Wr& w, CFuncArgs& args) {
        w.write_raw("(");
        for (auto& arg : args) {
            const auto need_comma = w.t.back() != '(';
            if (need_comma) {
                w.write_raw(",");
            }
            w.write_raw(arg.type_);
            if (arg.argname.size()) {
                w.write_raw(" ", arg.argname);
            }
        }
        w.write_raw(")");
    }

    void write_function(Wr& w, std::shared_ptr<CFunction>& stat) {
        write_start s{w, false};
        w.write_raw(stat->return_, " ", stat->identifier);
        write_arguments(w, stat->arguments);
        if (stat->block) {
            w.write_raw(" {");
            w.write_line();
            write_block(w, stat->block, false);
        }
        else {
            w.write_raw(";");
        }
    }

    void write_func_typedef(Wr& w, std::shared_ptr<CFuncTypedef>& stat) {
        write_start s{w};
        w.write_raw("typedef ", stat->return_, " ", "(*", stat->identifier, ")");
        write_arguments(w, stat->arguments);
    }

    void write_statements(Wr& w, Statements& statements) {
        for (auto& s : statements) {
            w.write_line();
            if (s->stkind == csk_return) {
                auto v = std::static_pointer_cast<CReturnStat>(s);
                write_return(w, v);
            }
            if (s->stkind == csk_func) {
                auto v = std::static_pointer_cast<CFunction>(s);
                write_function(w, v);
            }
            if (s->stkind == csk_functy) {
                auto v = std::static_pointer_cast<CFuncTypedef>(s);
                write_func_typedef(w, v);
            }
            if (s->stkind == csk_expr) {
                write_start ws{w};
                auto v = std::static_pointer_cast<CExpr>(s);
                write_expr(w, v);
            }
            if (s->stkind == csk_vardef) {
            }
        }
    }

    void write_c(C& c, std::string& buf) {
        Wr w{buf, "    "};
        write_statements(w, c.statements);
    }
}  // namespace minlc
