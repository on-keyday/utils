/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/wrap/cin.h"
#include "../../include/wrap/cout.h"
#include "../../include/wrap/iocommon.h"
#include <console/ansiesc.h>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#else
#include <unistd.h>
#define Sleep(n) usleep(n * 1000)
#endif

void test_cin() {
    auto& cout = utils::wrap::cout_wrap();
    cout.set_virtual_terminal(true);
    auto& cin = utils::wrap::cin_wrap();
    cout << "|>> ";
    size_t i = 0;
    size_t count = 0;
    utils::wrap::path_string peek, prev;
    namespace ces = utils::console::escape;
    bool updated = false;
    cout << ces::cursor_visibility(ces::CursorVisibility::hide);
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
            cout << ces::text_modify(ces::TextModify::delete_line, 1);
            update_progress();
            cout << ">> ";
            cout << peek;
            prev = peek;
        }
        else {
            cout << ces::cursor(ces::CursorMove::from_left, 0);
            update_progress();
        }
    }
    utils::wrap::string str;
    cin >> str;
    cout << "\n"
         << str;
    cout << ces::cursor_visibility(ces::CursorVisibility::show);
}

int main() {
    test_cin();
}
