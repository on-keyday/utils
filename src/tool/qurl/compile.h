/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/tree/branch_table.h>
#include <vector>
#include <helper/indent.h>
#include "runtime.h"
#include <comb2/tree/simple_node.h>
#include "error.h"
#include <list>

namespace qurl {
    namespace comb2 = futils::comb2;
    namespace compile {

        namespace node = comb2::tree::node;

        using CodeW = futils::helper::CodeWriter<std::string, std::string_view>;
        inline void write_nodes(CodeW &w, const std::shared_ptr<comb2::tree::Element> &elm) {
            if (auto id = comb2::tree::is_Ident<const char *>(elm)) {
                w.writeln(id->tag, ": ", id->ident);
            }
            else if (auto v = comb2::tree::is_Group<const char *>(elm)) {
                w.writeln(v->tag, ":");
                const auto s = w.indent_scope();
                for (auto &e : v->child) {
                    write_nodes(w, e);
                }
            }
            else if (auto v = comb2::tree::is_Branch(elm)) {
                for (auto &e : v->child) {
                    write_nodes(w, e);
                }
            }
        }

        inline void write_asm(CodeW &w, std::vector<runtime::Instruction> &istrs) {
            const auto s = w.indent_scope();
            w.should_write_indent(true);
            for (auto &istr : istrs) {
                w.write(to_string(istr.op), " ", istr.value.to_string());
                w.writeln(" ;pos={",
                          futils::number::to_string<std::string>(istr.pos.begin), ",",
                          futils::number::to_string<std::string>(istr.pos.end), "}");
                if (auto fn = istr.value.get<std::shared_ptr<runtime::Func>>()) {
                    write_asm(w, (*fn)->istrs);
                }
            }
        }

        struct Env {
            std::vector<runtime::Instruction> istrs;
            std::vector<Error> errors;
            std::uint64_t label_index = 0;

            void error(Error &&err) {
                errors.push_back(std::move(err));
            }
            std::uint64_t get_label() {
                return label_index;
            }
        };

        bool compile(Env &env, const std::shared_ptr<node::Group> &node);

    }  // namespace compile
}  // namespace qurl
