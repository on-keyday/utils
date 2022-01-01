/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

#include "../../include/helper/appender.h"

namespace binred {
    namespace hlp = utils::helper;
    void generate_cpp(utw::string& str, FileData& data, GenFlag flag) {
        for (auto& def : data.defvec) {
            auto d = *data.structs.find(def);
            hlp::appends(str, "struct ", d.first, " {\n");
            for (auto& memb : d.second.member) {
                hlp::appends(str, memb.type.name, " ", memb.name);
                if (memb.defval.size()) {
                    hlp::appends(str, " = ", memb.defval);
                }
                hlp::append(str, ";");
            }
            hlp::append(str, "};\n");
        }
    }
}  // namespace binred