/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/file/file_view.h"
#include "../../include/core/sequencer.h"

#include <filesystem>

void test_file_view() {
    utils::file::View file;
    // Assum:
    // current directry is ${workspaceRoot}/
    // target file ${workspaceRoot}/src/test/file/test.txt is exists

    assert(std::filesystem::exists("src/test/file/test.txt") && "expected file not exists");

    file.open("src/test/file/test.txt");
    assert(file.is_open() && "file not opened");
    utils::Sequencer<utils::file::View&> view(file);

    assert(view.seek_if(u8"𠮷野家") && "file seek failed");
    assert(view.match("\r\n") && "expect to open on binary mode but not");
}

int main() {
    test_file_view();
}
