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
                hlp::appends(str, "    ", memb.type.name, " ", memb.name);
                if (memb.defval.size()) {
                    hlp::appends(str, " = ", memb.defval);
                }
                hlp::append(str, ";\n");
            }
            hlp::append(str, "};\n\n");
            hlp::appends(str, "template<class Output>\nbool encode(", d.first, "& input,OutPut& output){\n");
            for (auto& memb : d.second.member) {
                auto& flag = memb.type.flag;
                auto has_flag = flag.type != FlagType::none;
                if (has_flag) {
                    if (flag.type == FlagType::eq) {
                        hlp::appends(str, "    if (", flag.depend, " == ", flag.val, ") {\n    ");
                    }
                    else if (flag.type == FlagType::bit) {
                        hlp::appends(str, "    if (", flag.depend, "&", flag.val, ") {\n    ");
                    }
                }
                hlp::appends(str, "    output.", "write", "(", memb.name, ");\n");
                if (has_flag) {
                    hlp::append(str, "    }\n");
                }
            }
            hlp::append(str, "}\n\n");
        }
    }
}  // namespace binred