/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "subcommand.h"
#include "../../include/file/file_view.h"
#include "../../include/helper/splits.h"
#include <fstream>
using namespace utils;

namespace netutil {
    wrap::string* file;
    wrap::string* out;
    void debug_log_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("mdump", debug_log_parse, "parse debug memory dump and analize it");
        file = subcmd->option().String<wrap::string>("input,i", "", "input file", "FILE");
        out = subcmd->option().String<wrap::string>("output,o", "", "output file (optional)", "FILE");
    }

    int debug_log_parse(subcmd::RunCommand& ctx) {
        file::View view;
        if (!view.open(*file)) {
            cout << ctx.cuc() << "error: failed to open " << *file;
            return -1;
        }
        auto lines = helper::lines(view);
        view.close();
        std::map<std::uint32_t, wrap::string> ids;
        auto cout = wrap::pack();
        for (auto& line : lines) {
            auto splt = helper::make_ref_splitview(line, "/");
            auto parse_num = [&](auto&& tgt) {
                auto seq = make_cpy_seq(helper::make_ref_splitview(tgt, ":")[1]);
                helper::space::consume_space(seq);
                std::size_t id = 0;
                number::parse_integer(seq, id);
                return id;
            };
            if (helper::equal(splt[0], "malloc:")) {
                ids.emplace(parse_num(splt[2]), std::move(line));
            }
            else {
                auto num = parse_num(splt[2]);
                auto found = ids.find(num);
                if (found != ids.end()) {
                    auto del_c = parse_num(splt[1]);
                    auto v = helper::make_ref_splitview(found->second, "/");
                    auto alo_c = parse_num(v[1]);
                    cout << "pair  req: " << num << "\n";
                    if (del_c != alo_c) {
                        cout << "warning: delete size and allocate size not match\n";
                    }
                    cout << found->second << "\n";
                    cout << line << "\n";
                    cout << "time delta: " << parse_num(splt[4]) - parse_num(v[4]) << "\n";
                }
                else {
                    cout << "warning: delete but not allocated req: " << num << "\n"
                         << line << "\n";
                }
                ids.erase(num);
            }
        }
        ::cout << cout.pack();
        if (out->size()) {
            std::ofstream fs(*out);
            if (!fs) {
                ::cout << "error: failed to open output file " << *out;
                return -1;
            }
            fs << utf::convert<wrap::string>(cout.raw());
        }
        return 0;
    }
}  // namespace netutil
