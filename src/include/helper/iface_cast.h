/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iface_cast - interface cast for class generated by ifacegen
#pragma once

#include <typeinfo>

namespace utils {
    namespace helper {

        template <class T, class In>
        T* iface_cast(In* in) {
            return in->template type_assert<T>();
        }

        template <class T, class In>
        T iface_cast(In& in) {
            auto res = in.template type_assert<T>();
            if (!res) {
                throw std::bad_cast();
            }
            return *res;
        }

    }  // namespace helper
}  // namespace utils
