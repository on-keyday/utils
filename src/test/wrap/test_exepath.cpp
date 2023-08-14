/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <wrap/exepath.h>
#include <filesystem>
#include <wrap/cout.h>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path path = utils::wrap::get_exepath<std::string>();
    utils::wrap::cout_wrap() << path;
}