/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <file/file.h>

int main() {
    utils::file::File file;
    file = utils::file::File::open("test.txt", utils::file::O_READ, utils::file::r_perm).value();
    auto m = file.mmap(utils::file::r_perm).value();
    auto text = m.read_view();
}