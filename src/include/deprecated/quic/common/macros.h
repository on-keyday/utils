/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// macros - quic macros
#pragma once
#define illegal()           \
    int* illegal = nullptr; \
    *illegal = 0
#define unreachable(why) __builtin_unreachable()