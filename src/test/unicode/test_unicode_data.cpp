/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <unicode/data/unicodedata_txt.h>
#include <unicode/data/unicodedata_bin.h>
#include <file/file_view.h>
#include <set>

int main() {
    namespace unicodedata = utils::unicode::data;
    utils::file::View file;
    if (!file.open("ignore/UnicodeData.txt")) {
        return -1;
    }
    auto seq = utils::make_ref_seq(file);
    unicodedata::UnicodeData data, red;
    auto res = unicodedata::text::parse_unicodedata_text(seq, data);
    assert(res == unicodedata::text::ParseError::none);
    std::string binary;
    utils::io::expand_writer<std::string&> w{binary};
    unicodedata::bin::serialize_unicodedata(w, data);
    utils::io::reader r{binary};
    auto ok = unicodedata::bin::deserialize_unicodedata(r, red);
    assert(ok);
    size_t dec_max = 0;
    std::set<std::string> cmd;
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
    }
}