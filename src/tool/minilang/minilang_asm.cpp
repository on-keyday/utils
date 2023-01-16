/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "minilang.h"

namespace minilang {
    namespace assembly {
        bool NodeAnalyzer::collect_symbols(Node* node) {
            node->data = new AsmData{};
            auto collect_each = [&](int start = 0) {
                for (auto i = 0; i < length(node->children); i++) {
                    if (!node->child(i)) {
                        continue;
                    }
                    if (!collect_symbols(node->child(i))) {
                        return false;
                    }
                }
            };
            if (is(node->expr, "program") || is(node->expr, "block")) {
                return collect_each();
            }
            else if (is(node->expr, "let")) {
                auto ptr = static_cast<expr::LetExpr<wrap::string>*>(node->expr);
                node->data->instance = new Instance{};
                node->data->instance->base = node;
                node->data->instance->instance = 0;
            }
        }

    }  // namespace assembly
}  // namespace minilang
