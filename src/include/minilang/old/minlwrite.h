/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlwrite - minilang reverse source
#pragma once
#include "minnode.h"
#include "../helper/appender.h"
#include "../helper/equal.h"
#include "../helper/indent.h"

namespace utils {
    namespace minilang {
        namespace writers {
            template <class T>
            using IW = helper::IndentWriter<T>;

            constexpr const char* simple_prefix_type(auto& str, bool space = false) {
                if (helper::equal(str, const_str_)) {
                    if (space) {
                        return "const ";
                    }
                    return "const";
                }
                if (helper::equal(str, mut_str_)) {
                    if (space) {
                        return "mut ";
                    }
                    return "mut";
                }
                if (helper::equal(str, pointer_str_)) {
                    return "*";
                }
                if (helper::equal(str, va_arg_str_)) {
                    return "...";
                }
                if (helper::equal(str, vector_str_)) {
                    return "[]";
                }
                return nullptr;
            }

            template <class T>
            void write_type(IW<T>& w, const std::shared_ptr<TypeNode>& node, bool omit_param) {
                if (!node) {
                    return;
                }
                if (auto pr = simple_prefix_type(node->str, true)) {
                    w.write_raw(pr);
                    write_type(w, node->type);
                    return;
                }
                if (auto fn = is_Func(node)) {
                    if (fn->mode = fe_tydec) {
                        w.write_raw("fn (");
                        bool first = true;
                        auto cur = fn->next;
                        auto write_comma = [&]() {
                            if (first) {
                                first = false;
                            }
                            else {
                                w.write_raw(",");
                            }
                        };
                        while (cur) {
                            const auto count = std::count(cur->ident->str.begin(), cur->ident->str.end(), ',') + 1;
                            if (omit_param) {
                                for (auto i = 0; i < count; i++) {
                                    write_comma();
                                    write_type(w, cur->type, omit_param);
                                }
                            }
                            else {
                                write_comma();
                                if (cur->ident) {
                                    w.write_raw(cur->ident->str, " ");
                                }
                                write_type(w, cur->type, omit_param);
                            }
                            cur = cur->next;
                        }
                        w.write(")");
                    }
                }
            }
        }  // namespace writers
    }      // namespace minilang
}  // namespace utils
