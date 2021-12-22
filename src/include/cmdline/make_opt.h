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
#include "../wrap/lite/map.h"

namespace utils {
    namespace cmdline {
        template <class T, template <class...> class Vec = wrap::vector>
        VecOption<Vec, T> multi_option(size_t maximum, size_t miniimum = ~0, Vec<T>&& def = Vec<T>{}) {
            VecOption<Vec, T> ret;
            ret.defval.resize(maximum);
            ret.minimum = miniimum;
            for (size_t i = 0; i < maximum && i < def.size(); i++) {
                ret.defval[i] = std::move(def[i]);
            }
            return ret;
        }

        template <class String = wrap::string, class T>
        String str_option(T&& v) {
            return utf::convert<String>(v);
        }

        template <class Int = std::int64_t>
        std::int64_t int_option(Int i) {
            return static_cast<std::int64_t>(i);
        }

        template <class Bool = bool>
        bool bool_option(Bool b) {
            return static_cast<bool>(b);
        }

        using DefaultDesc = OptionDesc<wrap::string, wrap::vector, wrap::map>;
        using DefaultSet = OptionSet<wrap::string, wrap::vector, wrap::map>;

    }  // namespace cmdline
}  // namespace utils
