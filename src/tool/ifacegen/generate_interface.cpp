/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "interface_list.h"
#include "../../include/helper/appender.h"

namespace ifacegen {
    namespace hlp = utils::helper;

    bool generate_cpp(FileData& data, utw::string& str) {
        hlp::append(str, R"(
// Code generated by ifacegen (https://github.com/on-keyday/utils) DO NOT EDIT.

#pragma once
#include<sfinae.h>
)");
        if (data.pkgname.size()) {
            hlp::append(str, "namespace ");
            hlp::append(str, data.pkgname);
            hlp::append(str, " {\n");
        }
        for (auto& iface : data.ifaces) {
            hlp::append(str, "    struct ");
            hlp::append(str, iface.first);
            hlp::append(str, " {\n");
            hlp::append(str, R"(
    
            )");
        }
    }

    bool generate(FileData& data, utw::string& str, Language lang) {
    }
}  // namespace ifacegen