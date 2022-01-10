/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/wrap/cin.h"
#include "../../include/wrap/cout.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#define Sleep sleep
#endif
void test_cin() {
    auto& cout = utils::wrap::cout_wrap();
    auto& cin = utils::wrap::cin_wrap();
    while (!cin.has_input()) {
        Sleep(1);
        cout << "waiting\n";
    }
    utils::wrap::string str;
    cin >> str;
    cout << str;
}

int main() {
    test_cin();
}