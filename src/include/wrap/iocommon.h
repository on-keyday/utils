/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iocommon - io common wrap
#pragma once
#include "../platform/windows/dllexport_header.h"

namespace utils {
    namespace wrap {
        utils_DLL_EXPORT extern int stdinmode;
        utils_DLL_EXPORT extern int stdoutmode;
        utils_DLL_EXPORT extern int stderrmode;
        utils_DLL_EXPORT extern bool sync_stdio;
        utils_DLL_EXPORT extern bool no_change_mode;
        utils_DLL_EXPORT extern bool out_virtual_terminal;
        utils_DLL_EXPORT extern bool need_cr_for_return;
        utils_DLL_EXPORT extern bool in_virtual_terminal;
        utils_DLL_EXPORT extern bool handle_input_self;
        utils_DLL_EXPORT void STDCALL force_init_io();
    }  // namespace wrap
}  // namespace utils
