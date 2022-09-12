/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minlc.h"
#include <minilang/minlutil.h>
#include <string_view>
#include <vector>
#include <map>
namespace minlc {
    namespace hlp = utils::helper;
    bool collect_global_symbols(pTSymbols& symbols, const pNode& symbol, ErrC& errc) {
        if (!symbol || symbol->str != mi::program_str_) {
            return false;
        }
        return mi::collect_toplevel_symbol(symbols, symbol, errc);
    }

    std::shared_ptr<mi::MinNode> gentype(const char* type, ErrC& errc) {
        auto tes = parse(utils::make_cpy_seq(std::string("mod test type Test ") + type), errc);
        if (!tes.second) {
            errc.say("library bug!!!");
            return nullptr;
        }
        auto block = mi::is_Block(tes.second);
        auto def = mi::is_TypeDef(block->element);
        return def->next->type;
    }

    bool mapping_c_linkage_main(pFnNode& node, C& c, ErrC& errc) {
        if (!mi::equal_Type(gentype("fn(mut int,mut**char) (int)", errc), node)) {
            errc.say("c linkage `main` must be `fn(argc mut int,argv mut **char) int`");
            errc.node(node);
            return false;
        }
        for (auto& decs : c.symbols->merged.func_decs) {
            for (auto& dec : decs.second) {
                c.add_function(errc, dec.node, dec.linkage);
            }
        }
        auto main_ = c.add_function(errc, node, mi::mlink_C);
        return true;
    }

    bool mapping_min_to_c_from_main(C& c, ErrC& errc) {
        auto& mains = c.symbols->merged.func_defs["main"];
        if (mains.size() == 0) {
            errc.say("no `main` function found");
            return false;
        }
        auto check_dup = mi::duplicate_func_checker(errc);
        if (!check_dup("main", c.symbols->native, c.symbols->c, c.symbols->file, c.symbols->mod)) {
            return false;
        }
        std::shared_ptr<mi::FuncNode> main_node;
        mi::MinLinkage linkage = mi::mlink_file;
        for (auto& main : mains) {
            if (main.linkage == mi::mlink_C) {
                main_node = main.node;
                linkage = mi::mlink_C;
                break;
            }
            else if (main.linkage == mi::mlink_native) {
                main_node = main.node;
                linkage = mi::mlink_native;
            }
            else if (linkage != mi::mlink_native && main.linkage == mi::mlink_mod) {
                main_node = main.node;
                linkage = mi::mlink_mod;
            }
            else if (linkage == mi::mlink_file && main.linkage == mi::mlink_file) {
                main_node = main.node;
            }
        }
        if (linkage == mi::mlink_file) {
            errc.say("main function with file linkage is not allowed");
            errc.node(main_node);
            return false;
        }
        if (linkage == mi::mlink_C) {
            return mapping_c_linkage_main(main_node, c, errc);
        }
        errc.say("unimplemented now");
        return false;
    }

}  // namespace minlc
