/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iocommon - io common wrap
#pragma once
#include "../platform/windows/dllexport_header.h"

namespace utils {
    namespace wrap {
        DLL extern int stdinmode;
        DLL extern int stdoutmode;
        DLL extern int stderrmode;
        DLL extern bool sync_stdio;
        DLL extern bool no_change_mode;
        DLL extern bool out_virtual_terminal;
        DLL extern bool need_cr_for_return;
        DLL extern bool in_virtual_terminal;
        DLL extern bool handle_input_self;
        DLL void STDCALL force_init_io();
    }  // namespace wrap
}  // namespace utils
