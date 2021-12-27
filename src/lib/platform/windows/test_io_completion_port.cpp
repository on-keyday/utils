/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/io_completetion_port.h"

void test_io_completion_port() {
    auto iocp = utils::platform::windows::start_iocp();
    iocp->register_handler();
}

int main() {
    test_io_completion_port();
}