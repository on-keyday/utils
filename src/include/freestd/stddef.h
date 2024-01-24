/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#if defined(_WIN32)  // TODO(on-keyday): what is freestanding?
typedef unsigned long long size_t;
typedef long long ptrdiff_t;
#else
typedef unsigned long size_t;
typedef long ptrdiff_t;
#endif
