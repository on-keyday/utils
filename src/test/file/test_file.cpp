/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <file/file.h>

int main() {
    futils::file::File file;
    file = futils::file::File::open("test.txt", futils::file::O_READ, futils::file::r_perm).value();
    auto m = file.mmap(futils::file::r_perm).value();
    auto text = m.read_view();
}