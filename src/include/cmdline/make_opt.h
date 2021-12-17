/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// make_opt - make option
#pragma once

#include "optdef.h"

#include "../wrap/lite/string.h"
#include "../wrap/lite/vector.h"

namespace utils {
    namespace cmdline {
        template <class T, template <class...> class Vec = wrap::vector>
        VecOption<Vec, T> multivalue(size_t maximum, size_t miniimum = ~0, Vec<T>&& def = Vec<T>{}) {
            VecOption<Vec, T> ret;
            ret.defval.resize(maximum);
            ret.minimum = miniimum;
            for (size_t i = 0; i < maximum; i++) {
                ret.defval[i] = std::move(def[i]);
            }
            return ret;
        };
    }  // namespace cmdline
}  // namespace utils