/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/wrap/cin.h"
#include "../../include/wrap/cout.h"
#include "../../include/wrap/iocommon.h"
#include "../../include/helper/view.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#define Sleep sleep
#endif

void test_cin() {
    utils::wrap::out_virtual_terminal = true;
    auto& cout = utils::wrap::cout_wrap();
    auto& cin = utils::wrap::cin_wrap();
    cout << "|>> ";
    size_t i = 0;
    size_t count = 0;
    utils::wrap::path_string peek, prev;
    bool updated = false;
    cout << "\e[?25l";
    while (!cin.peek_buffer(peek, false, &updated)) {
        Sleep(1);
        auto update_progress = [&] {
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
            if (count > 10) {
                i++;
                if (i == 4) {
                    i = 0;
                }
                count = 0;
            }
            else {
                count++;
            }
        };

        if (updated) {
            cout << "\e[1M";
            cout << utils::helper::CharView<wchar_t>('\b', prev.size() + 1);
            cout << "\b\b\b\b";
            update_progress();
            cout << ">> ";
            cout << peek;
            prev = peek;
        }
        else {
            cout << "\e[0G";
            update_progress();
        }
    }
    utils::wrap::string str;
    cin >> str;
    cout << str;
    cout << "\e[?25h";
}

int main() {
    test_cin();
}
