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
    cout << "|>> ";
    size_t i = 0;
    utils::wrap::path_string peek;
    while (!cin.peek_buffer(peek)) {
        Sleep(100);
        cout << "\b\b\b\b";
        switch (i) {
            case 0:
                cout << "\\";
                break;
            case 1:
                cout << "-";
                break;
            case 2:
                cout << "/";
                break;
            case 3:
                cout << "|";
                break;
        }
        cout << ">> ";
        i++;
        if (i == 4) {
            i = 0;
        }
    }
    utils::wrap::string str;
    cin >> str;
    cout << str;
}

int main() {
    test_cin();
}