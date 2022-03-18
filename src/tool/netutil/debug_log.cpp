/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/file/file_view.h"
#include "../../include/helper/splits.h"
using namespace utils;

namespace netutil {
    wrap::string* file;
    void debug_log_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("mdump", debug_log_parse, "parse debug memory dump and analize it");
        subcmd->option().String<wrap::string>("input,i", "", "input file", "FILE");
    }

    int debug_log_parse(subcmd::RunCommand& ctx) {
        file::View view;
        if (!view.open(*file)) {
            cout << ctx.cuc() << "error: failed to open " << *file;
        }
        auto lines = helper::lines(view);
        std::map<std::uint32_t, wrap::string> ids;
        for (auto& line : lines) {
            auto splt = helper::make_ref_splitview(line, "/");
            auto parse_num = [&]() {
                auto seq = make_cpy_seq(helper::make_cpy_splitview(splt[2], ":")[1]);
                helper::space::consume_space(seq);
                std::uint32_t id = 0;
                number::parse_integer(seq, id);
                return id;
            };
            if (helper::equal(splt[0], "malloc:")) {
                ids.emplace(parse_num(), std::move(line));
            }
            else {
                ids.erase(parse_num());
            }
        }
        return 0;
    }
}  // namespace netutil
