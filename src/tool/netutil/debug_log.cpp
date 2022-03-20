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
    enum class FileFormat {
        original,
        csv,
    } format;
    void debug_log_option(subcmd::RunContext& ctx) {
        auto subcmd = ctx.SubCommand("mdump", debug_log_parse, "parse debug memory dump and analize it");
        common_option(*subcmd);
        file = subcmd->option().String<wrap::string>("input,i", "", "input file", "FILE");
        out = subcmd->option().String<wrap::string>("output,o", "", "output file (optional)", "FILE");
        cmdline::option::MappingParser<wrap::string, FileFormat, wrap::map> mpparser;
        mpparser.mapping["original"] = FileFormat::original;
        mpparser.mapping["csv"] = FileFormat::csv;
        subcmd->option().Option("format", &format, mpparser, "output format", "original|csv");
    }

    auto parse_num(auto&& tgt) {
        auto seq = make_cpy_seq(helper::make_ref_splitview(tgt, ":")[1]);
        helper::space::consume_space(seq);
        std::int64_t id = 0;
        number::parse_integer(seq, id);
        return id;
    };

    void print_as_csv(std::uint32_t id, auto& cout, auto& aloc, auto& deloc) {
        // reqid,alloc_size,free_size,alloc_time,free_time,
        // time_delta,alloc_total,free_total,alloc_count,free_count
        auto alloc_size = parse_num(aloc[1]);
        auto deloc_size = parse_num(deloc[1]);
        auto alloc_time = parse_num(aloc[4]);
        auto deloc_time = parse_num(deloc[4]);
        auto time_delta = deloc_time - alloc_time;
        auto alloc_total = parse_num(aloc[3]);
        auto deloc_total = parse_num(deloc[3]);
        auto alloc_count = parse_num(aloc[5]);
        auto deloc_count = parse_num(deloc[5]);
        cout << id << "," << alloc_size << "," << deloc_size << "," << alloc_time << "," << deloc_time
             << "," << time_delta << "," << alloc_total << "," << deloc_total << "," << alloc_count
             << "," << deloc_count << "\n";
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
        if (format == FileFormat::csv) {
            cout << "reqid,alloc_size,free_size,alloc_time,free_time,time_delta,alloc_total,free_total,alloc_count,free_count\n";
        }
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
                    if (format == FileFormat::original) {
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
                    else if (format == FileFormat::csv) {
                        auto v = helper::make_ref_splitview(found->second, "/");
                        print_as_csv(found->first, cout, v, splt);
                    }
                }
                else {
                    if (format == FileFormat::original) {
                        cout << "warning: delete but not allocated req: " << num << "\n"
                             << line << "\n";
                    }
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
