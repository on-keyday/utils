/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// writer
#pragma once
#include <string>
#include <helper/indent.h>
#include "vstack.h"
#include "ssa.h"

namespace vstack {
    namespace write {
        struct Context {
            std::string vstack_type;
        };

        struct StackTree {
            StackTree* parent = nullptr;
            StackTree** rep;
            std::string name;

            StackTree(auto&& prefix, StackTree*& p)
                : name(prefix), parent(p), rep(std::addressof(p)) {
                p = this;
            }

            std::string prefix() const {
                if (parent) {
                    return parent->prefix() + name;
                }
                return name;
            }

            std::string modifname() const {
                auto pre = prefix();
                pre.pop_back();  // remove _
                return pre;
            }

            ~StackTree() {
                *rep = parent;
            }
        };

        struct Writer {
            utils::helper::IndentWriter<std::string> w;
            Writer()
                : w("", "    ") {
            }

            void merge_from(Writer& o) {
                w.write_raw(o.w.t);
                o.w.t.clear();
            }

            std::string string() const {
                return w.t;
            }
        };

        struct GlobalWriter {
            Writer src;
            Writer header;
        };

        struct Stacks {
            GlobalWriter global;
            StackTree* current;
            Context ctx;
            symbol::Refs* refs;
        };

        struct FuncStack : StackTree {
            std::shared_ptr<Func> fn;

            FuncStack(StackTree*& parent, const std::shared_ptr<Func>& f)
                : fn(f), StackTree("func_", parent) {
                name += fn->ident->token + "_";
            }

            void write_head(Writer& w, Context& ctx, bool need_ident, size_t index = ~0) {
                // now all types are int
                w.w.write_indent();
                w.w.write_raw("int ", symbol::qualified_ident(fn->ident));
                if (index != ~0) {
                    w.w.write_raw("_", utils::number::to_string<std::string>(index, 16));
                }
                w.w.write_raw("(", ctx.vstack_type, "*");
                if (need_ident) {
                    w.w.write_raw(" vstack");
                }
                for (size_t i = 0; i < fn->args.size(); i++) {
                    w.w.write_raw(", ");
                    if (need_ident) {
                        w.w.write_raw("int ", symbol::qualified_ident(fn->args[i]));
                    }
                    else {
                        w.w.write_raw("int");
                    }
                }
                w.w.write_raw(")");
            }

            void write_define(Writer& w, Context& ctx, size_t index = ~0) {
                write_head(w, ctx, true, index);
                w.w.write_raw(" ");
            }

            void write_declare(Writer& w, Context& ctx, size_t index = ~0) {
                write_head(w, ctx, false, index);
                w.w.write_raw(";\n\n");
            }

            const std::shared_ptr<token::Block<>>& block() {
                return fn->block;
            }
        };
    }  // namespace write

    namespace gen {
        void write_ssa(write::Stacks& s, write::Writer& l, ssa::SSA<>& ssa);
    }  // namespace gen
}  // namespace vstack
