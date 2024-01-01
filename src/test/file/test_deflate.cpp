/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <file/file_view.h>
#include <file/gzip/gzip.h>
#include <cassert>
#include <wrap/cout.h>

struct Input {
    futils::file::View& file;
    std::string str;
    size_t pos = 0;

    const char* data() const {
        return str.data();
    }

    size_t size() const {
        return str.size();
    }

    void input(size_t expect) {
        auto i = 0;
        for (; pos < file.size() && i < expect; pos++, i++) {
            str.push_back(file[pos]);
        }
    }
};

int main() {
    futils::file::View view;
    if (!view.open("./ignore/dump0.gz")) {
        return -1;
    }
    Input input{view};
    futils::file::gzip::GZipHeader head;
    futils::binary::bit_reader<Input&> bitr{input};
    std::string out;
    auto err = futils::file::gzip::decode_gzip(out, head, bitr);
    assert(err == futils::file::gzip::deflate::DeflateError::none);
    futils::wrap::cout_wrap() << out;
}
