/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <wrap/exepath.h>
#include <filesystem>
#include <wrap/cout.h>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path path = futils::wrap::get_exepath<std::string>();
    futils::wrap::cout_wrap() << path;
}