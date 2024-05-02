/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once


namespace futils::color {
    template<class T>
    struct RGB {
        T r{};
        T g{};
        T b{};
    };

    template<class T>
    struct RGBA :RGB {
        T a{};
    };
}
