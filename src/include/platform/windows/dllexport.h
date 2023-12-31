/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// dllexport - define dllexport flag
#pragma once
// Be enable below if you build utils as dll on windows
#if !defined(UTILS_AS_STATIC) && defined(_WIN32)
#define UTILS_AS_DLL
#endif
