/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <unicode/data/unicodedata_txt.h>
#include <unicode/data/unicodedata_bin.h>
#include <file/file_view.h>
#include <set>
#include <wrap/cout.h>
#include <algorithm>

int main() {
    namespace unicodedata = futils::unicode::data;
    futils::file::View file, file2, file3;
    if (!file.open("ignore/UnicodeData.txt")) {
        return -1;
    }
    if (!file2.open("ignore/Blocks.txt")) {
        return -1;
    }
    if (!file3.open("ignore/emoji-data.txt")) {
        return -1;
    }
    auto seq = futils::make_ref_seq(file);
    unicodedata::UnicodeData data;
    auto res = unicodedata::text::parse_unicodedata_text(seq, data);
    assert(res == unicodedata::text::ParseError::none);
    auto seq2 = futils::make_ref_seq(file2);
    res = unicodedata::text::parse_blocks_text(seq2, data);
    assert(res == unicodedata::text::ParseError::none);
    auto seq3 = futils::make_ref_seq(file3);
    res = unicodedata::text::parse_emoji_data_text(seq3, data);
    assert(res == unicodedata::text::ParseError::none);
    std::string binary;
    futils::binary::expand_writer<std::string&> w{binary};
    unicodedata::bin::serialize_unicodedata(w, data);
    futils::binary::reader r{binary};
    unicodedata::UnicodeData<futils::view::rvec> red;
    auto ok = unicodedata::bin::deserialize_unicodedata(r, red);
    assert(ok);
    size_t dec_max = 0;
    std::set<futils::view::rvec> cmd;
    std::set<futils::view::rvec> blocks;
    std::vector<unicodedata::CodeInfo<futils::view::rvec>*> emojis;
    for (auto& code : red.codes) {
        auto v = code.second.decomposition.to.size();
        if (v) {
            if (v > dec_max) {
                dec_max = v;
            }
        }
        if (code.second.decomposition.command.size()) {
            cmd.emplace(code.second.decomposition.command);
        }
        if (code.second.block.size()) {
            blocks.emplace(code.second.block);
        }
        if (code.second.emoji_data.size()) {
            if (std::find(code.second.emoji_data.begin(), code.second.emoji_data.end(), "Emoji") != code.second.emoji_data.end()) {
                emojis.push_back(&code.second);
            }
            else {
                ;
            }
        }
    }
    auto& cout = futils::wrap::cout_wrap();
    for (auto& s : emojis) {
        char32_t d[] = {s->codepoint, 0};
        cout << s->block << ":" << s->name << ":" << std::u32string_view(d) << ":" << std::uint32_t(s->codepoint) << "\n";
    }
    auto huji = red.lookup(U'𠮷');
}