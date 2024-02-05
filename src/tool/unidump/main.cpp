/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <wrap/argv.h>
#include <unicode/data/unicodedata_txt.h>
#include <unicode/data/unicodedata_bin.h>
#include <fstream>
#include <fcntl.h>
enum class Output {
    dec_array,
    binary,
    like_hex_dump,
};

struct Flags : futils::cmdline::templ::HelpOption {
    std::string unicode_data;
    std::string blocks_data;
    std::string east_asian_width_data;
    std::string emoji_data;
    std::string output;
    Output mode = Output::dec_array;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&unicode_data, "u,unicode-data", "set unicodedata.txt location", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString(&blocks_data, "b,blocks", "set Blocks.txt location", "FILE");
        ctx.VarString(&east_asian_width_data, "a,east-asian", "set EastAsianWIdth.txt location", "FILE");
        ctx.VarString(&emoji_data, "e,emoji", "set emoji-data.txt location", "FILE");
        ctx.Option("m,output-mode", &mode, futils::cmdline::option::MappingParser<std::string, Output, std::map>{
                                               .mapping = {
                                                   {"array", Output::dec_array},
                                                   {"visible", Output::like_hex_dump},
                                                   {"binary", Output::binary},
                                               },
                                           },
                   "output mode (array,visible,binary) (default:array)", "MODE");
        ctx.VarString(&output, "o,output", "output file", "FILE");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();
namespace unicodedata = futils::unicode::data;

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    futils::file::View file;
    if (!file.open(flags.unicode_data)) {
        cerr << "cannnot open file: " << flags.unicode_data << "\n";
        return -1;
    }
    auto seq = futils::make_ref_seq(file);
    unicodedata::UnicodeData data;
    auto err = unicodedata::text::parse_unicodedata_text(seq, data);
    if (err != unicodedata::text::ParseError::none) {
        cerr << "failed to parse unicodedata.txt file: " << flags.unicode_data << "\n";
        return -1;
    }
    if (flags.east_asian_width_data.size()) {
        file.close();
        if (!file.open(flags.east_asian_width_data)) {
            cerr << "cannnot open file: " << flags.east_asian_width_data << "\n";
            return -1;
        }
        seq.rptr = 0;
        err = unicodedata::text::parse_east_asian_width_text(seq, data);
        if (err != unicodedata::text::ParseError::none) {
            cerr << "failed to parse EastAsianWidth.txt file: " << flags.east_asian_width_data << "\n";
            return -1;
        }
    }
    if (flags.blocks_data.size()) {
        file.close();
        if (!file.open(flags.blocks_data)) {
            cerr << "cannnot open file: " << flags.blocks_data << "\n";
            return -1;
        }
        seq.rptr = 0;
        err = unicodedata::text::parse_blocks_text(seq, data);
        if (err != unicodedata::text::ParseError::none) {
            cerr << "failed to parse Blocks.txt file: " << flags.blocks_data << "\n";
            return -1;
        }
    }
    if (flags.emoji_data.size()) {
        file.close();
        if (!file.open(flags.emoji_data)) {
            cerr << "cannnot open file: " << flags.emoji_data << "\n";
            return -1;
        }
        seq.rptr = 0;
        err = unicodedata::text::parse_emoji_data_text(seq, data);
        if (err != unicodedata::text::ParseError::none) {
            cerr << "failed to parse emoji-data.txt file: " << flags.emoji_data << "\n";
            return -1;
        }
    }
    std::string out;
    futils::binary::expand_writer<std::string&> w{out};
    if (!unicodedata::bin::serialize_unicodedata(w, data)) {
        cerr << "failed to serialize unicode data\n";
        return -1;
    }
    if (flags.output.size()) {
        std::ofstream fs(flags.output, std::ios::binary);
        if (!fs) {
            cerr << "failed to open output file: " << flags.output << "\n";
            return -1;
        }
        if (flags.mode == Output::like_hex_dump) {
            cerr << "mode=visible is not supported for file output\n";
            return -1;
        }
        if (flags.mode == Output::binary) {
            fs << out;
        }
        else {
            for (auto& c : out) {
                fs << futils::number::to_string<std::string>(futils::byte(c)) << ",";
            }
        }
        return 0;
    }
    if (flags.mode == Output::binary) {
        cerr << "mode=binary is not supported for console output\nif you want to output to file,use -o option";
        return -1;
    }
    if (flags.mode == Output::like_hex_dump) {
        for (auto& c : out) {
            if (c >= 0x20 && c <= 0x7e) {
                cout << c;
            }
            else {
                cout << ".";
            }
        }
    }
    else {
        for (auto& c : out) {
            cout << futils::number::to_string<std::string>(futils::byte(c)) << ",";
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
