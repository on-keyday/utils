/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "type.h"

namespace oslbgen {
    Pos write_code(Out& out, DataSet& db) {
        auto it = db.dataset.begin();
        for (; it != db.dataset.end(); it++) {
            auto& code = *it;
#define SWITCH() \
    if (false) { \
    }
#define CASE(T) else if (auto t = std::get_if<T>(&code))
            SWITCH()
            CASE(Comment) {
                out.write_unformatted(t->comment);
                if (!t->comment.ends_with("\n")) {
                    out.line();
                }
            }
            CASE(Preproc) {
                out.should_write_indent(false);
                out.write(t->directive);
            }
            else {
                break;
            }
        }
        auto base = it;
        out.line();
        out.writeln("namespace ", db.ns, " {");
        const auto d = futils::helper::defer([&] {
            out.writeln("} // namespace ", db.ns);
        });
        const auto sc = out.indent_scope();
        using ScopePtr = decltype(out.indent_scope_ex());
        struct NestScope {
            ScopePtr scope;
            std::string ns;
        };
        std::vector<NestScope> scope;
        auto clear_scope = [&] {
            while (scope.size()) {
                auto ns = std::move(scope.back().ns);
                scope.pop_back();
                out.writeln("} // namespace ", ns);
            }
            out.line();
        };
        auto add_scope = [&](auto&& ns) {
            out.writeln("namespace ", ns, " {");
            scope.push_back(NestScope{
                .scope = out.indent_scope_ex(),
                .ns = ns,
            });
        };
        Spec spec;
        {
            auto prev_idx = -1;
            for (; it != db.dataset.end(); it++) {
                auto& code = *it;
                if (prev_idx != code.index() &&
                    prev_idx != cmt_index) {
                    out.line();
                }

                SWITCH()
                CASE(Comment) {
                    out.write_unformatted(t->comment);
                    if (!t->comment.ends_with("\n")) {
                        out.line();
                    }
                }
                CASE(Preproc) {
                    out.should_write_indent(false);
                    out.write(t->directive);
                }
                CASE(Use) {
                    out.writeln("using ", t->use, " = struct ", t->use, ";");
                }
                CASE(Alias) {
                    out.writeln("using ", t->name, " = ", t->token, ";");
                }
                CASE(Define) {
                    if (t->is_fn_style) {
                        out.should_write_indent(false);
                        out.write("#define ", t->macro_name, "(");
                        for (auto i = 0; i < t->args.size(); i++) {
                            if (i != 0) {
                                out.write(", ");
                            }
                            out.write(t->args[i]);
                        }
                        out.write(") ", t->token);
                        out.line();
                    }
                    else {
                        out.writeln("constexpr auto ", t->macro_name, "_ = ", t->token, ";");
                    }
                }
                CASE(Fn) {
                    out.writeln(t->ret, " ", t->fn_name, t->args, ";");
                }
                CASE(Strc) {
                    out.write(t->type, " ", t->name, " ");
                    out.write_unformatted(t->inner);
                    out.writeln(";");
                }
                CASE(Spec) {
                    clear_scope();
                    if (t->spec != "no") {
                        add_scope(t->spec);
                        for (auto& c : t->targets) {
                            add_scope(c);
                        }
                    }
                    spec = *t;
                }
                prev_idx = code.index();
            }
        }
        clear_scope();
        it = base;
        return npos;
    }
}  // namespace oslbgen
