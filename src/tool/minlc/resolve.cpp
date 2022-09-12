/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "middle.h"

namespace minlc {
    namespace resolver {
        bool resolve(middle::M& m) {
            namespace md = middle;
            for (auto& sym : m.object_mapping) {
                if (sym.second->obj_type == md::mk_ident) {
                    auto ident = std::static_pointer_cast<md::IdentExpr>(sym.second);
                    if (ident->lookedup.size() == 0) {
                        m.errc.say("identifier `", ident->node->str, "` is not found");
                        m.errc.node(ident->node);
                        return false;
                    }
                }
                if (sym.second->obj_type == md::mk_dot_expr) {
                    auto dot = std::static_pointer_cast<md::DotExpr>(sym.second);
                    if (dot->lookedup.size() == 0) {
                        if (dot->dots[0]->str == "C") {  // c lang linkage
                        }
                    }
                }
            }
            return true;
        }
    }  // namespace resolver
}  // namespace minlc
