/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// dllexport - define dllexport flag
#pragma once
// Be enable below if you build futils as dll on windows
#if !defined(FUTILS_AS_STATIC) && defined(_WIN32)
#define FUTILS_AS_DLL
#endif
